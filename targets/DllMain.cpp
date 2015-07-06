#include "Main.hpp"
#include "Thread.hpp"
#include "DllHacks.hpp"
#include "DllOverlayUi.hpp"
#include "DllNetplayManager.hpp"
#include "ChangeMonitor.hpp"
#include "SmartSocket.hpp"
#include "UdpSocket.hpp"
#include "Exceptions.hpp"
#include "Enum.hpp"
#include "ErrorStringsExt.hpp"
#include "KeyboardState.hpp"
#include "CharacterSelect.hpp"
#include "SpectatorManager.hpp"
#include "DllControllerManager.hpp"
#include "DllFrameRate.hpp"
#include "ReplayManager.hpp"
#include "DllRollbackManager.hpp"

#include <windows.h>

#include <vector>
#include <memory>
#include <algorithm>

using namespace std;


// The main log file path
#define LOG_FILE                    FOLDER "dll.log"

// The number of milliseconds to poll for events each frame
#define POLL_TIMEOUT                ( 3 )

// The extra number of frames to delay checking round over state during rollback
#define ROLLBACK_ROUND_OVER_DELAY   ( 5 )

// The number of milliseconds to wait for the initial connect
#define INITIAL_CONNECT_TIMEOUT     ( 60000 )

// The number of milliseconds to wait to perform a delayed stop so that ErrorMessages are received before sockets die
#define DELAYED_STOP                ( 100 )

// The number of milliseconds before resending inputs while waiting for more inputs
#define RESEND_INPUTS_INTERVAL      ( 100 )

// The maximum number of milliseconds to wait for inputs before timeout
#define MAX_WAIT_INPUTS_INTERVAL    ( 10000 )

// The maximum number of spectators allowed for ClientMode::Spectate
#define MAX_SPECTATORS              ( 15 )

// The maximum number of spectators allowed for ClientMode::Host/Client
#define MAX_ROOT_SPECTATORS         ( 1 )

// Indicates if this client should redirect spectators
#define SHOULD_REDIRECT_SPECTATORS  ( clientMode.isSpectate()                                                       \
                                      ? numSpectators() >= MAX_SPECTATORS                                           \
                                      : numSpectators() >= MAX_ROOT_SPECTATORS )


#define LOG_SYNC(FORMAT, ...)                                                                                       \
    LOG_TO ( syncLog, "%s [%u] %s [%s] " FORMAT,                                                                    \
             gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,                                                \
             netMan.getState(), netMan.getIndexedFrame(), ## __VA_ARGS__ )

#define LOG_SYNC_CHARACTER(N)                                                                                       \
    LOG_SYNC ( "P%u: C=%u; M=%u; c=%u; seq=%u; st=%u; hp=%u; rh=%u; gb=%.1f; gq=%.1f; mt=%u; ht=%u; x=%d; y=%d",    \
               N, *CC_P ## N ## _CHARACTER_ADDR, *CC_P ## N ## _MOON_SELECTOR_ADDR,                                 \
               *CC_P ## N ## _COLOR_SELECTOR_ADDR, *CC_P ## N ## _SEQUENCE_ADDR, *CC_P ## N ## _SEQ_STATE_ADDR,     \
               *CC_P ## N ## _HEALTH_ADDR, *CC_P ## N ## _RED_HEALTH_ADDR, *CC_P ## N ## _GUARD_BAR_ADDR,           \
               *CC_P ## N ## _GUARD_QUALITY_ADDR,  *CC_P ## N ## _METER_ADDR, *CC_P ## N ## _HEAT_ADDR,             \
               *CC_P ## N ## _X_POSITION_ADDR, *CC_P ## N ## _Y_POSITION_ADDR )


// Main application state
static ENUM ( AppState, Uninitialized, Polling, Stopping, Deinitialized ) appState = AppState::Uninitialized;

// Main application instance
struct DllMain;
static shared_ptr<DllMain> main;

// Mutex for deinitialize()
static Mutex deinitMutex;
static void deinitialize();

// Enum of variables to monitor
ENUM ( Variable, WorldTime, GameMode, RoundStart,
       MenuConfirmState, AutoReplaySave, GameStateCounter, CurrentMenuIndex );

// Global stopping flag
bool stopping = false;


struct DllMain
    : public Main
    , public RefChangeMonitor<Variable, uint32_t>::Owner
    , public PtrToRefChangeMonitor<Variable, uint32_t>::Owner
    , public SpectatorManager
    , public DllControllerManager
{
    // NetplayManager instance
    NetplayManager netMan;

    // DllRollbackManager instance
    DllRollbackManager rollMan;

    // If remote has loaded up to character select
    bool remoteCharaSelectLoaded = false;

    // ChangeMonitor for CC_WORLD_TIMER_ADDR
    RefChangeMonitor<Variable, uint32_t> worldTimerMoniter;

    // Timer for resending inputs while waiting
    TimerPtr resendTimer;

    // Timer for waiting for inputs
    int waitInputsTimer = -1;

    // Indicates if we should sync the game RngState on this frame
    bool shouldSyncRngState = false;

    // Frame to stop on, when fast-forwarding the game.
    // Used as a flag to indicate fast-forward mode, 0:0 means not fast-forwarding.
    IndexedFrame fastFwdStopFrame = {{ 0, 0 }};

    // Initial connect timer
    TimerPtr initialTimer;

    // Local player inputs
    array<uint16_t, 2> localInputs = {{ 0, 0 }};

    // If we have sent our local retry menu index
    bool localRetryMenuIndexSent = false;

    // If we should disconnect at the next NetplayState change
    bool lazyDisconnect = false;

    // If the delay and/or rollback should be changed
    bool shouldChangeDelayRollback = false;

    // Latest ChangeConfig for changing delay/rollback
    ChangeConfig changeConfig;

    // Client serverCtrlSocket address
    IpAddrPort clientServerAddr;

    // Sockets that have been redirected to another client
    unordered_set<Socket *> redirectedSockets;

    // Timer to delay checking round over state during rollback
    int roundOverTimer = -1;

    // We should only rollback if this timer is full
    int rollbackTimer = 0;

    // If we should fast-forward when spectating
    bool spectateFastFwd = true;

    // The minimum number of frames that must run normally, before we're allowed to do another rollback
    uint8_t minRollbackSpacing = 2;

#ifndef RELEASE
    // Local and remote SyncHashes
    list<MsgPtr> localSync, remoteSync;

    // Debug testing flags
    bool randomInputs = false;
    bool randomDelay = false;
    bool randomRollback = false;
    uint32_t rollUpTo = 10;

    // ReplayManager instance
    ReplayManager repMan;
    bool replayInputs = false;
    uint32_t replaySpeed = 2;
    IndexedFrame replayStop = MaxIndexedFrame;
    IndexedFrame replayCheck = MaxIndexedFrame;
    string replayCheckRngHexStr;
#endif // NOT RELEASE

    void frameStepNormal()
    {
        switch ( netMan.getState().value )
        {
            case NetplayState::PreInitial:
            case NetplayState::Initial:
            case NetplayState::AutoCharaSelect:
                // Skip rendering while loading character select
                *CC_SKIP_FRAMES_ADDR = 1;
                break;

            case NetplayState::InGame:
                if ( netMan.getRollback() )
                {
                    // Only save rollback states in-game
                    rollMan.saveState ( netMan );

                    // Delayed round over check
                    if ( roundOverTimer > 0 )
                        --roundOverTimer;
                }

            case NetplayState::CharaSelect:
            case NetplayState::Loading:
            case NetplayState::Skippable:
            case NetplayState::RetryMenu:
            {
                // Fast-forward if spectator
                if ( spectateFastFwd && clientMode.isSpectate() && netMan.getState() != NetplayState::Loading )
                {
                    static bool doneSkipping = true;

                    const IndexedFrame remoteIndexedFrame = netMan.getRemoteIndexedFrame();

                    // Fast-forward implemented by skipping the rendering every other frame
                    if ( doneSkipping && remoteIndexedFrame.value > netMan.getIndexedFrame().value + 2 * NUM_INPUTS )
                    {
                        *CC_SKIP_FRAMES_ADDR = 1;
                        doneSkipping = false;
                    }
                    else if ( !doneSkipping && *CC_SKIP_FRAMES_ADDR == 0 )
                    {
                        doneSkipping = true;
                    }
                }

                // Update controller state once per frame
                KeyboardState::update();
                updateControls ( &localInputs[0] );

                if ( DllOverlayUi::isEnabled() )                                            // Overlay UI controls
                {
                    localInputs[0] = localInputs[1] = 0;
                }
                else if ( clientMode.isNetplay() || clientMode.isLocal() )                  // Netplay + local controls
                {
                    if ( KeyboardState::isDown ( VK_CONTROL ) )
                    {
                        for ( uint8_t delay = 0; delay < 10; ++delay )
                        {
                            if ( delay == netMan.getDelay() )
                                continue;

                            if ( KeyboardState::isPressed ( '0' + delay )                   // Ctrl + Number
                                    || KeyboardState::isPressed ( VK_NUMPAD0 + delay ) )    // Ctrl + Numpad Number
                            {
                                shouldChangeDelayRollback = true;

                                changeConfig.value = ChangeConfig::Delay;
                                changeConfig.indexedFrame = netMan.getIndexedFrame();
                                changeConfig.delay = delay;
                                changeConfig.rollback = netMan.getRollback();
                                changeConfig.invalidate();
                                break;
                            }
                        }
                    }

                    if ( KeyboardState::isDown ( VK_MENU ) && netMan.getRollback() )        // Only if already rollback
                    {
                        for ( uint8_t rollback = 1; rollback < 10; ++rollback )             // Don't allow 0 rollback
                        {
                            if ( rollback == netMan.getRollback() )
                                continue;

                            if ( KeyboardState::isPressed ( '0' + rollback )                // Alt + Number
                                    || KeyboardState::isPressed ( VK_NUMPAD0 + rollback ) ) // Alt + Numpad Number
                            {
                                shouldChangeDelayRollback = true;

                                changeConfig.value = ChangeConfig::Rollback;
                                changeConfig.indexedFrame = netMan.getIndexedFrame();
                                changeConfig.delay = netMan.getDelay();
                                changeConfig.rollback = rollback;
                                changeConfig.invalidate();
                                break;
                            }
                        }
                    }

#ifndef RELEASE
                    // Test random delay setting
                    if ( KeyboardState::isPressed ( VK_F11 ) )
                    {
                        randomDelay = !randomDelay;
                        DllOverlayUi::showMessage ( randomDelay ? "Enabled random delay" : "Disabled random delay" );
                    }

                    if ( randomDelay && rand() % 30 == 0 )
                    {
                        shouldChangeDelayRollback = true;
                        changeConfig.indexedFrame = netMan.getIndexedFrame();
                        changeConfig.delay = rand() % 10;
                        changeConfig.invalidate();
                    }
#endif // NOT RELEASE
                }
                else if ( clientMode.isSpectate() )                                         // Spectator controls
                {
                    if ( KeyboardState::isPressed ( VK_SPACE ) )
                        spectateFastFwd = !spectateFastFwd;
                }
                else
                {
                    LOG ( "Unknown clientMode=%s; flags={ %s }", clientMode, clientMode.flagString() );
                    break;
                }

#ifndef RELEASE
                DllOverlayUi::debugText = format ( "%+d [%s]", netMan.getRemoteFrameDelta(), netMan.getIndexedFrame() );
                DllOverlayUi::debugTextAlign = 1;

                // Replay inputs and rollback
                if ( replayInputs )
                {
                    if ( repMan.getGameMode ( netMan.getIndexedFrame() ) )
                        ASSERT ( repMan.getGameMode ( netMan.getIndexedFrame() ) == *CC_GAME_MODE_ADDR );

                    if ( !repMan.getStateStr ( netMan.getIndexedFrame() ).empty() )
                        ASSERT ( repMan.getStateStr ( netMan.getIndexedFrame() ) == netMan.getState().str() );

                    // Inputs
                    const auto& inputs = repMan.getInputs ( netMan.getIndexedFrame() );
                    netMan.setInput ( 1, inputs.p1 );
                    netMan.setInput ( 2, inputs.p2 );

                    const IndexedFrame target = repMan.getRollbackTarget ( netMan.getIndexedFrame() );

                    // Rollback
                    if ( netMan.isInRollback() && target.value < netMan.getIndexedFrame().value )
                    {
                        // Reinputs
                        const auto& reinputs = repMan.getReinputs ( netMan.getIndexedFrame() );
                        for ( const auto& inputs : reinputs )
                        {
                            netMan.assignInput ( 1, inputs.p1, inputs.indexedFrame );
                            netMan.assignInput ( 2, inputs.p2, inputs.indexedFrame );
                        }

                        const string before = format ( "%s [%u] %s [%s]",
                                                       gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,
                                                       netMan.getState(), netMan.getIndexedFrame() );

                        // Indicate we're re-running to the current frame
                        fastFwdStopFrame = netMan.getIndexedFrame();

                        // Reset the game state (this resets game state AND netMan state)
                        if ( rollMan.loadState ( target, netMan ) )
                        {
                            // Start fast-forwarding now
                            *CC_SKIP_FRAMES_ADDR = 1;

                            LOG_TO ( syncLog, "%s Rollback: target=[%s]; actual=[%s]",
                                     before, target, netMan.getIndexedFrame() );

                            LOG_SYNC ( "Reinputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );
                            return;
                        }

                        LOG_TO ( syncLog, "%s Rollback to target=[%s] failed!", before, target );

                        ASSERT_IMPOSSIBLE;
                    }

                    // RngState
                    if ( netMan.getFrame() == 0 && ( netMan.getState() == NetplayState::CharaSelect
                                                     || netMan.getState() == NetplayState::InGame ) )
                    {
                        MsgPtr msgRngState = repMan.getRngState ( netMan.getIndexedFrame() );

                        if ( msgRngState )
                            procMan.setRngState ( msgRngState->getAs<RngState>() );
                    }

                    break;
                }

                // Test random input
                if ( KeyboardState::isPressed ( VK_F12 ) )
                {
                    randomInputs = !randomInputs;
                    localInputs [ clientMode.isLocal() ? 1 : 0 ] = 0;
                    DllOverlayUi::showMessage ( randomInputs ? "Enabled random inputs" : "Disabled random inputs" );
                }

                if ( randomInputs )
                {
                    bool shouldRandomize = ( rand() % 2 );
                    if ( netMan.isInRollback() )
                        shouldRandomize &= ( netMan.getFrame() % 150 < 120 );

                    if ( shouldRandomize )
                    {
                        uint16_t direction = ( rand() % 10 );

                        // Reduce the chances of moving the cursor at retry menu
                        if ( netMan.getState() == NetplayState::RetryMenu && ( rand() % 2 ) )
                            direction = 0;

                        uint16_t buttons = ( rand() % 0x1000 );

                        // Reduce the chances of hitting the D button
                        if ( rand() % 100 < 98 )
                            buttons &= ~ CC_BUTTON_D;

                        // Prevent hitting some non-essential buttons
                        buttons &= ~ ( CC_BUTTON_FN1 | CC_BUTTON_FN2 | CC_BUTTON_START );

                        // Prevent going back at character select
                        if ( netMan.getState() == NetplayState::CharaSelect )
                            buttons &= ~ ( CC_BUTTON_B | CC_BUTTON_CANCEL );

                        localInputs [ clientMode.isLocal() ? 1 : 0 ] = COMBINE_INPUT ( direction, buttons );
                    }
                }
#endif // NOT RELEASE

                // Assign local player input
                if ( !clientMode.isSpectate() )
                {
#ifndef RELEASE
                    if ( netMan.isInRollback() )
                        netMan.assignInput ( localPlayer, localInputs[0], netMan.getFrame() + netMan.getDelay() );
                    else
#endif // NOT RELEASE
                        netMan.setInput ( localPlayer, localInputs[0] );
                }

                if ( clientMode.isNetplay() )
                {
                    // Special netplay retry menu behaviour, only select final option after both sides have selected
                    if ( netMan.getState() == NetplayState::RetryMenu )
                    {
                        MsgPtr msgMenuIndex = netMan.getLocalRetryMenuIndex();

                        // Lazy disconnect now once the retry menu option has been selected
                        if ( msgMenuIndex && ( !dataSocket || !dataSocket->isConnected() ) )
                        {
                            if ( lazyDisconnect )
                            {
                                lazyDisconnect = false;
                                delayedStop ( "Disconnected!" );
                            }
                            break;
                        }

                        // Only send retry menu index once
                        if ( msgMenuIndex && !localRetryMenuIndexSent )
                        {
                            localRetryMenuIndexSent = true;
                            dataSocket->send ( msgMenuIndex );
                        }
                        break;
                    }

                    dataSocket->send ( netMan.getInputs ( localPlayer ) );
                }
                else if ( clientMode.isLocal() )
                {
                    netMan.setInput ( remotePlayer, localInputs[1] );
                }

                if ( shouldSyncRngState && ( clientMode.isHost() || clientMode.isBroadcast() ) )
                {
                    shouldSyncRngState = false;

                    MsgPtr msgRngState = procMan.getRngState ( netMan.getIndex() );

                    ASSERT ( msgRngState.get() != 0 );

                    netMan.setRngState ( msgRngState->getAs<RngState>() );

                    if ( clientMode.isHost() )
                        dataSocket->send ( msgRngState );
                }
                break;
            }

            default:
                ASSERT ( !"Unknown NetplayState!" );
                break;
        }

        // Clear the last changed frame before we get new inputs
        if ( rollbackTimer == minRollbackSpacing )
            netMan.clearLastChangedFrame();

        for ( ;; )
        {
            // Poll until we are ready to run
            if ( !EventManager::get().poll ( POLL_TIMEOUT ) )
            {
                appState = AppState::Stopping;
                return;
            }

            // Don't need to wait for anything in local modes
            if ( clientMode.isLocal() || lazyDisconnect )
                break;

            // Check if we are ready to continue running, ie not waiting on remote input or RngState
            const bool ready = ( netMan.isRemoteInputReady() && netMan.isRngStateReady ( shouldSyncRngState ) );

            // Don't resend inputs in spectator mode
            if ( clientMode.isSpectate() )
            {
                // Continue if ready
                if ( ready )
                    break;
            }
            else
            {
                // Stop resending inputs if we're ready
                if ( ready )
                {
                    resendTimer.reset();
                    waitInputsTimer = -1;
                    break;
                }

                // Start resending inputs since we are waiting
                if ( !resendTimer )
                {
                    resendTimer.reset ( new Timer ( this ) );
                    resendTimer->start ( RESEND_INPUTS_INTERVAL );
                    waitInputsTimer = 0;
                }
            }
        }

        if ( rollbackTimer < minRollbackSpacing )
        {
            --rollbackTimer;

            if ( rollbackTimer < 0 )
                rollbackTimer = minRollbackSpacing;
        }

        // Only rollback when necessary
        if ( netMan.isInRollback()
                && rollbackTimer == minRollbackSpacing
                && netMan.getLastChangedFrame().value < netMan.getIndexedFrame().value )
        {
            const string before = format ( "%s [%u] %s [%s]",
                                           gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,
                                           netMan.getState(), netMan.getIndexedFrame() );

            // Indicate we're re-running to the current frame
            fastFwdStopFrame = netMan.getIndexedFrame();

            // Reset the game state (this resets game state AND netMan state)
            if ( rollMan.loadState ( netMan.getLastChangedFrame(), netMan ) )
            {
                // Start fast-forwarding now
                *CC_SKIP_FRAMES_ADDR = 1;

                LOG_TO ( syncLog, "%s Rollback: target=[%s]; actual=[%s]",
                         before, netMan.getLastChangedFrame(), netMan.getIndexedFrame() );

                LOG_SYNC ( "Reinputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );

                netMan.clearLastChangedFrame();
                --rollbackTimer;
                return;
            }

            LOG_TO ( syncLog, "%s Rollback to target=[%s] failed!", before, netMan.getLastChangedFrame() );
        }

        // Update the RngState if necessary
        if ( shouldSyncRngState )
        {
            shouldSyncRngState = false;

            // LOG ( "Randomizing RngState" );

            // RngState rngState;

            // rngState.rngState0 = rand() % 1000000;
            // rngState.rngState1 = rand() % 1000000;
            // rngState.rngState2 = rand() % 1000000;

            // for ( char& c : rngState.rngState3 )
            //     c = rand() % 256;

            // procMan.setRngState ( rngState );

            MsgPtr msgRngState = netMan.getRngState();

            if ( msgRngState )
                procMan.setRngState ( msgRngState->getAs<RngState>() );
        }

        // Update delay and/or rollback if necessary
        if ( shouldChangeDelayRollback )
        {
            shouldChangeDelayRollback = false;

            if ( changeConfig.delay < 0xFF && changeConfig.delay != netMan.getDelay() )
            {
                LOG ( "Input delay was changed %u -> %u", netMan.getDelay(), changeConfig.delay );
                DllOverlayUi::showMessage ( format ( "Input delay was changed to %u", changeConfig.delay ) );
                netMan.setDelay ( changeConfig.delay );
                procMan.ipcSend ( changeConfig );
            }

            if ( changeConfig.rollback <= MAX_ROLLBACK && changeConfig.rollback != netMan.getRollback() )
            {
                LOG ( "Rollback was changed %u -> %u", netMan.getRollback(), changeConfig.rollback );
                DllOverlayUi::showMessage ( format ( "Rollback was changed to %u", changeConfig.rollback ) );
                netMan.setRollback ( changeConfig.rollback );
                minRollbackSpacing = clamped<uint8_t> ( netMan.getRollback(), 2, 4 );
                procMan.ipcSend ( changeConfig );
            }
        }

        // LOG_SYNC ( "SFX 0x%X: CC_SFX_ARRAY=%u; sfxFilterArray=%u; sfxMuteArray=%u", SFX_NUM,
        //            CC_SFX_ARRAY_ADDR[SFX_NUM], AsmHacks::sfxFilterArray[SFX_NUM], AsmHacks::sfxMuteArray[SFX_NUM] );

#ifndef RELEASE
        if ( !replayInputs )
        {
            // Test one time rollback
            if ( KeyboardState::isPressed ( VK_F9 ) && netMan.isInGame() )
            {
                IndexedFrame target = netMan.getIndexedFrame();

                if ( target.parts.frame <= 30 )
                    target.parts.frame = 0;
                else
                    target.parts.frame -= 30;

                if ( KeyboardState::isDown ( VK_CONTROL ) )
                {
                    const string before = format ( "%s [%u] %s [%s]",
                                                   gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,
                                                   netMan.getState(), netMan.getIndexedFrame() );

                    // Indicate we're re-running to the current frame
                    fastFwdStopFrame = netMan.getIndexedFrame();

                    // Reset the game state (this resets game state AND netMan state)
                    if ( rollMan.loadState ( target, netMan ) )
                    {
                        // Start fast-forwarding now
                        *CC_SKIP_FRAMES_ADDR = 1;

                        LOG_TO ( syncLog, "%s Rollback: target=[%s]; actual=[%s]",
                                 before, netMan.getLastChangedFrame(), netMan.getIndexedFrame() );

                        LOG_SYNC ( "Reinputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );
                        return;
                    }
                }
                else
                {
                    rollMan.loadState ( target, netMan );
                }
            }

            // Test random rollback
            if ( KeyboardState::isPressed ( VK_F10 ) )
            {
                randomRollback = !randomRollback;
                DllOverlayUi::showMessage ( randomRollback ? "Enabled random rollback" : "Disabled random rollback" );
            }

            if ( randomRollback
                    && rollbackTimer == minRollbackSpacing
                    && netMan.isInGame()
                    && ( netMan.getFrame() % 150 < 100 ) )
            {
                const uint32_t distance = 1 + ( rand() % rollUpTo );

                IndexedFrame target = netMan.getIndexedFrame();

                if ( target.parts.frame <= distance )
                    target.parts.frame = 0;
                else
                    target.parts.frame -= distance;

                const string before = format ( "%s [%u] %s [%s]",
                                               gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,
                                               netMan.getState(), netMan.getIndexedFrame() );

                // Indicate we're re-running to the current frame
                fastFwdStopFrame = netMan.getIndexedFrame();

                // Reset the game state (this resets game state AND netMan state)
                if ( rollMan.loadState ( target, netMan ) )
                {
                    // Start fast-forwarding now
                    *CC_SKIP_FRAMES_ADDR = 1;

                    LOG_TO ( syncLog, "%s Rollback: target=[%s]; actual=[%s]",
                             before, target, netMan.getIndexedFrame() );

                    LOG_SYNC ( "Reinputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );

                    --rollbackTimer;
                    return;
                }

                LOG_TO ( syncLog, "%s Rollback to target=[%s] failed!", before, target );
            }
        }

        if ( dataSocket && dataSocket->isConnected()
                && ( ( netMan.getFrame() % ( 5 * 60 ) == 0 ) || ( netMan.getFrame() % 150 == 149 ) )
                && netMan.getState().value >= NetplayState::CharaSelect && netMan.getState() != NetplayState::Loading
                && netMan.getState() != NetplayState::Skippable && netMan.getState() != NetplayState::RetryMenu )
        {
            // Check for desyncs by periodically sending hashes
            if ( !netMan.isInRollback()
                    || ( netMan.getFrame() == 0 )
                    || ( randomInputs && netMan.getFrame() % 150 == 149 ) )
            {
                MsgPtr msgSyncHash ( new SyncHash ( netMan.getIndexedFrame() ) );
                dataSocket->send ( msgSyncHash );
                localSync.push_back ( msgSyncHash );
            }
        }

        // Compare current lists of sync hashes
        while ( !localSync.empty() && !remoteSync.empty() )
        {

#define L localSync.front()->getAs<SyncHash>()
#define R remoteSync.front()->getAs<SyncHash>()

            while ( !remoteSync.empty() && L.indexedFrame.value > R.indexedFrame.value )
                remoteSync.pop_front();

            if ( remoteSync.empty() )
                break;

            while ( !localSync.empty() && R.indexedFrame.value > L.indexedFrame.value )
                localSync.pop_front();

            if ( localSync.empty() )
                break;

            if ( L == R )
            {
                localSync.pop_front();
                remoteSync.pop_front();
                continue;
            }

            LOG_TO ( syncLog, "Desync:" );
            LOG_TO ( syncLog, "< %s", L.dump() );
            LOG_TO ( syncLog, "> %s", R.dump() );

#undef L
#undef R

            syncLog.deinitialize();
            delayedStop ( "Desync!" );

            randomInputs = false;
            localInputs [ clientMode.isLocal() ? 1 : 0 ] = 0;
            return;
        }

        if ( replayInputs && netMan.getIndex() >= repMan.getLastIndex() && netMan.getFrame() >= repMan.getLastFrame() )
        {
            replayInputs = false;
            SetForegroundWindow ( ( HWND ) DllHacks::windowHandle );
        }

        if ( netMan.getIndexedFrame().value == replayStop.value )
            MessageBox ( 0, 0, 0, 0 );

        if ( netMan.getIndexedFrame().value == replayCheck.value )
        {
            MsgPtr msgRngState = procMan.getRngState ( 0 );
            ASSERT ( msgRngState.get() != 0 );

            const string& dump = msgRngState->getAs<RngState>().dump();

            if ( dump.find ( replayCheckRngHexStr ) != 0 )
            {
                LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );
                LOG_TO ( syncLog, "Desync!" );
                syncLog.deinitialize();

                delayedStop ( ERROR_INTERNAL );
                return;
            }
            else
            {
                delayedStop ( ERROR_INTERNAL );
                return;
            }
        }

        // if ( netMan.getIndex() == 1802 && netMan.getFrame() == 460 )
        // {
        //     if ( *CC_P1_HEALTH_ADDR != 11400 || *CC_P1_METER_ADDR != 0
        //             || *CC_P2_HEALTH_ADDR != 10121 || *CC_P2_METER_ADDR != 10638 )
        //     {
        //         LOG_SYNC_CHARACTER ( 1 );
        //         LOG_SYNC_CHARACTER ( 2 );
        //         LOG_TO ( syncLog, "Desync!" );
        //         syncLog.deinitialize();

        //         MessageBox ( 0, 0, 0, 0 );
        //         return;
        //     }
        //     else
        //     {
        //         delayedStop ( ERROR_INTERNAL );
        //         return;
        //     }
        // }
#endif // NOT RELEASE

        // Cleared last played and muted sound effects
        memset ( AsmHacks::sfxFilterArray, 0, CC_SFX_ARRAY_LEN );
        memset ( AsmHacks::sfxMuteArray, 0, CC_SFX_ARRAY_LEN );

#ifndef DISABLE_LOGGING
        MsgPtr msgRngState = procMan.getRngState ( 0 );
        ASSERT ( msgRngState.get() != 0 );

        // Log state every frame
        LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );
        LOG_SYNC ( "Inputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );

        // Log extra state during chara select
        if ( netMan.getState() == NetplayState::CharaSelect )
        {
            LOG_SYNC ( "P1: sel=%u; C=%u; M=%u; c=%u; P2: sel=%u; C=%u; M=%u; c=%u",
                       *CC_P1_SELECTOR_MODE_ADDR, *CC_P1_CHARACTER_ADDR,
                       *CC_P1_MOON_SELECTOR_ADDR, *CC_P1_COLOR_SELECTOR_ADDR,
                       *CC_P2_SELECTOR_MODE_ADDR, *CC_P2_CHARACTER_ADDR,
                       *CC_P2_MOON_SELECTOR_ADDR, *CC_P2_COLOR_SELECTOR_ADDR );
            return;
        }

        // Log extra state while in-game
        if ( netMan.isInGame() )
        {
            LOG_SYNC_CHARACTER ( 1 );
            LOG_SYNC_CHARACTER ( 2 );
            LOG_SYNC ( "roundOverTimer=%d; introState=%u; roundTimer=%u; realTimer=%u; hitsparks=%u; camera={ %d, %d }",
                       roundOverTimer, *CC_INTRO_STATE_ADDR, *CC_ROUND_TIMER_ADDR, *CC_REAL_TIMER_ADDR,
                       *CC_HIT_SPARKS_ADDR, *CC_CAMERA_X_ADDR, *CC_CAMERA_Y_ADDR );
            return;
        }
#endif // NOT DISABLE_LOGGING
    }

    void frameStepRerun()
    {
        // Here we don't save any game states while re-running because the inputs are faked

        // Save sound state during rollback re-run
        rollMan.saveRerunSounds ( netMan.getFrame() );

        if ( netMan.getIndexedFrame().value >= fastFwdStopFrame.value )
        {
            // Stop fast-forwarding once we're reached the frame we want
            fastFwdStopFrame.value = 0;

            // Re-enable regular rendering once done
            *CC_SKIP_FRAMES_ADDR = 0;

            // Finalize rollback sound effects
            rollMan.finishedRerunSounds();
        }
        else
        {
            // Skip rendering while fast-forwarding
            *CC_SKIP_FRAMES_ADDR = 1;
        }

        LOG_SYNC ( "Reinputs: 0x%04x 0x%04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );
        LOG_SYNC ( "roundOverTimer=%d; introState=%u; roundTimer=%u; realTimer=%u; hitsparks=%u; camera={ %d, %d }",
                   roundOverTimer, *CC_INTRO_STATE_ADDR, *CC_ROUND_TIMER_ADDR, *CC_REAL_TIMER_ADDR,
                   *CC_HIT_SPARKS_ADDR, *CC_CAMERA_X_ADDR, *CC_CAMERA_Y_ADDR );

        // LOG_SYNC ( "ReSFX 0x%X: CC_SFX_ARRAY=%u; sfxFilterArray=%u; sfxMuteArray=%u", SFX_NUM,
        //            CC_SFX_ARRAY_ADDR[SFX_NUM], AsmHacks::sfxFilterArray[SFX_NUM], AsmHacks::sfxMuteArray[SFX_NUM] );
    }

    void frameStep()
    {
        // New frame
        netMan.updateFrame();
        procMan.clearInputs();

        // Check for changes to important variables for state transitions
        ChangeMonitor::get().check();

        // Check for controller changes normally on Wine
        if ( ProcessManager::isWine() )
            ControllerManager::get().check();

        // Check for round over state during in-game
        if ( netMan.isInGame() )
            checkRoundOver();

        // Need to manually set the intro state to 0 during rollback
        if ( netMan.isInRollback() && netMan.getFrame() > CC_PRE_GAME_INTRO_FRAMES && *CC_INTRO_STATE_ADDR )
            *CC_INTRO_STATE_ADDR = 0;

        // Perform the frame step
        if ( fastFwdStopFrame.value )
            frameStepRerun();
        else
            frameStepNormal();

        // Update spectators
        frameStepSpectators();

        // Write game inputs
        procMan.writeGameInput ( localPlayer, netMan.getInput ( localPlayer ) );
        procMan.writeGameInput ( remotePlayer, netMan.getInput ( remotePlayer ) );

#ifndef RELEASE
        if ( replayInputs && ( replaySpeed == 1 || KeyboardState::isDown ( VK_SPACE ) ) )
            DllFrameRate::desiredFps = numeric_limits<double>::max();
        else if ( replayInputs && replaySpeed == 2 )
            *CC_SKIP_FRAMES_ADDR = 1;
#endif
    }

    void netplayStateChanged ( NetplayState state )
    {
        // Catch invalid transitions
        if ( !netMan.isValidNext ( state ) )
        {
            LOG_TO ( syncLog, "Desync!" );
            LOG_TO ( syncLog, "Invalid transition: %s -> %s", netMan.getState(), state );
            syncLog.deinitialize();

            delayedStop ( ERROR_INTERNAL );
            return;
        }

        // Close the overlay if not mapping
        if ( !DllOverlayUi::isShowingMessage() && isNotMapping() )
        {
            DllOverlayUi::disable();
        }

        // Leaving Initial or AutoCharaSelect
        if ( netMan.getState() == NetplayState::Initial || netMan.getState() == NetplayState::AutoCharaSelect )
        {
#ifdef RELEASE
            // Try to focus the game window
            SetForegroundWindow ( ( HWND ) DllHacks::windowHandle );
#endif // RELEASE

            // Enable controllers now
            if ( !ProcessManager::isWine() )
                ControllerManager::get().startHighFreqPolling();

            // Initialize the overlay now
            DllOverlayUi::init();
        }

        // Leaving Skippable
        if ( netMan.getState() == NetplayState::Skippable )
        {
            // Reset state variables
            roundOverTimer = -1;
            lazyDisconnect = false;
        }

        // Leaving Loading
        if ( netMan.getState() == NetplayState::Loading )
        {
            // Reset color loading state
            AsmHacks::numLoadedColors = 0;
        }

        // Entering InGame
        if ( state == NetplayState::InGame )
        {
            if ( netMan.getRollback() )
                rollMan.allocateStates();
        }

        // Leaving InGame
        if ( netMan.getState() == NetplayState::InGame )
        {
            if ( netMan.getRollback() )
                rollMan.deallocateStates();
        }

        // Entering CharaSelect OR entering InGame
        if ( !clientMode.isOffline() && ( state == NetplayState::CharaSelect || state == NetplayState::InGame ) )
        {
            // Indicate we should sync the RngState now
            shouldSyncRngState = true;
        }

        // Entering RetryMenu
        if ( state == NetplayState::RetryMenu )
        {
            // Lazy disconnect now during netplay
            lazyDisconnect = clientMode.isNetplay();

            // Reset retry menu index flag
            localRetryMenuIndexSent = false;
        }
        else if ( lazyDisconnect )
        {
            lazyDisconnect = false;

            // If not entering RetryMenu and we're already disconnected...
            if ( !dataSocket || !dataSocket->isConnected() )
            {
                delayedStop ( "Disconnected!" );
                return;
            }
        }

        // Update local state
        netMan.setState ( state );

        // Update remote index
        if ( dataSocket && dataSocket->isConnected() )
            dataSocket->send ( new TransitionIndex ( netMan.getIndex() ) );
    }

    void gameModeChanged ( uint32_t previous, uint32_t current )
    {
        if ( current == 0
                || current == CC_GAME_MODE_STARTUP
                || current == CC_GAME_MODE_OPENING
                || current == CC_GAME_MODE_TITLE
                || current == CC_GAME_MODE_MAIN
                || current == CC_GAME_MODE_LOADING_DEMO
                || ( previous == CC_GAME_MODE_LOADING_DEMO && current == CC_GAME_MODE_IN_GAME )
                || current == CC_GAME_MODE_HIGH_SCORES )
        {
            ASSERT ( netMan.getState() == NetplayState::PreInitial || netMan.getState() == NetplayState::Initial );
            return;
        }

        if ( netMan.getState() == NetplayState::Initial
#ifdef RELEASE
                && netMan.config.mode.isSpectate()
#else
                && ( netMan.config.mode.isSpectate() || replayInputs )
#endif // RELEASE
                && netMan.initial.netplayState > NetplayState::CharaSelect )
        {
            // Spectate mode needs to auto select characters if starting after CharaSelect
            netplayStateChanged ( NetplayState::AutoCharaSelect );
            return;
        }

        if ( current == CC_GAME_MODE_CHARA_SELECT )
        {
            netplayStateChanged ( NetplayState::CharaSelect );
            return;
        }

        if ( current == CC_GAME_MODE_LOADING )
        {
            netplayStateChanged ( NetplayState::Loading );
            return;
        }

        if ( current == CC_GAME_MODE_IN_GAME )
        {
            // Versus mode in-game starts with character intros, which is a skippable state
            if ( netMan.config.mode.isVersus() )
                netplayStateChanged ( NetplayState::Skippable );
            else
                netplayStateChanged ( NetplayState::InGame );
            return;
        }

        if ( current == CC_GAME_MODE_RETRY )
        {
            netplayStateChanged ( NetplayState::RetryMenu );
            return;
        }

        THROW_EXCEPTION ( "gameModeChanged(%u, %u)", ERROR_INVALID_GAME_MODE, previous, current );
    }

    void delayedStop ( const string& error )
    {
        if ( !error.empty() )
            procMan.ipcSend ( new ErrorMessage ( error ) );

        stopTimer.reset ( new Timer ( this ) );
        stopTimer->start ( DELAYED_STOP );

        stopping = true;
    }

    void checkRoundOver()
    {
        const bool isOver = ( ( *CC_P1_NO_INPUT_FLAG_ADDR ) && ( *CC_P2_NO_INPUT_FLAG_ADDR ) );

        if ( netMan.getRollback() )
        {
            if ( isOver )
            {
                if ( roundOverTimer == 0 )
                {
                    roundOverTimer = -1;
                    netplayStateChanged ( NetplayState::Skippable );
                }
                else if ( roundOverTimer < 0 )
                {
                    roundOverTimer = netMan.getRollback() + ROLLBACK_ROUND_OVER_DELAY;
                }
            }
            else
            {
                roundOverTimer = -1;
            }
        }
        else if ( isOver )
        {
            netplayStateChanged ( NetplayState::Skippable );
        }
    }

    // ChangeMonitor callback
    void hasChanged ( Variable var, uint32_t previous, uint32_t current ) override
    {
        switch ( var.value )
        {
            case Variable::WorldTime:
                frameStep();
                break;

            case Variable::GameMode:
                LOG ( "[%s] %s: previous=%u; current=%u", netMan.getIndexedFrame(), var, previous, current );
                gameModeChanged ( previous, current );
                break;

            case Variable::RoundStart:
                // In-game happens after round start, when players can start moving
                LOG ( "[%s] %s: previous=%u; current=%u", netMan.getIndexedFrame(), var, previous, current );
                netplayStateChanged ( NetplayState::InGame );
                break;

            default:
                LOG ( "[%s] %s: previous=%u; current=%u", netMan.getIndexedFrame(), var, previous, current );
                break;
        }
    }

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override
    {
        LOG ( "acceptEvent ( %08x )", serverSocket );

        if ( serverSocket == serverCtrlSocket.get() )
        {
            LOG ( "serverCtrlSocket->accept ( this )" );

            SocketPtr newSocket = serverCtrlSocket->accept ( this );

            LOG ( "newSocket=%08x", newSocket.get() );

            ASSERT ( newSocket != 0 );
            ASSERT ( newSocket->isConnected() == true );

            IpAddrPort redirectAddr;

            if ( SHOULD_REDIRECT_SPECTATORS )
                redirectAddr = getRandomRedirectAddress();

            if ( redirectAddr.port == 0 )
            {
                newSocket->send ( new VersionConfig ( clientMode ) );
            }
            else
            {
                redirectedSockets.insert ( newSocket.get() );
                newSocket->send ( new IpAddrPort ( redirectAddr ) );
            }

            pushPendingSocket ( this, newSocket );
        }
        else if ( serverSocket == serverDataSocket.get() && !dataSocket )
        {
            LOG ( "serverDataSocket->accept ( this )" );

            dataSocket = serverDataSocket->accept ( this );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            ASSERT ( dataSocket != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            netplayStateChanged ( NetplayState::Initial );

            initialTimer.reset();
        }
        else
        {
            LOG ( "Unexpected acceptEvent from serverSocket=%08x", serverSocket );
            serverSocket->accept ( 0 ).reset();
            return;
        }
    }

    void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );

        ASSERT ( dataSocket.get() != 0 );
        ASSERT ( dataSocket->isConnected() == true );

        dataSocket->send ( serverCtrlSocket->address );

        netplayStateChanged ( NetplayState::Initial );

        initialTimer.reset();
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "disconnectEvent ( %08x )", socket );

        if ( socket == dataSocket.get() )
        {
            if ( netMan.getState() == NetplayState::PreInitial )
            {
                dataSocket = SmartSocket::connectUDP ( this, address );
                LOG ( "dataSocket=%08x", dataSocket.get() );
                return;
            }

            if ( lazyDisconnect )
                return;

            delayedStop ( "Disconnected!" );
            return;
        }

        redirectedSockets.erase ( socket );
        popPendingSocket ( socket );
        popSpectator ( socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "readEvent ( %08x, %s, %s )", socket, msg, address );

        if ( !msg.get() )
            return;

        if ( redirectedSockets.find ( socket ) != redirectedSockets.end() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::VersionConfig:
            {
                const Version RemoteVersion = msg->getAs<VersionConfig>().version;

                if ( !LocalVersion.similar ( RemoteVersion, 1 + options[Options::StrictVersion] ) )
                {
                    string local = LocalVersion.code;
                    string remote = RemoteVersion.code;

                    if ( options[Options::StrictVersion] >= 2 )
                    {
                        local += " " + LocalVersion.revision;
                        remote += " " + RemoteVersion.revision;
                    }

                    if ( options[Options::StrictVersion] >= 3 )
                    {
                        local += " " + LocalVersion.buildTime;
                        remote += " " + RemoteVersion.buildTime;
                    }

                    LOG ( "Incompatible versions:\nLocal version: %s\nRemote version: %s", local, remote );

                    socket->disconnect();
                    return;
                }

                socket->send ( new SpectateConfig ( netMan.config, netMan.getState().value ) );
                return;
            }

            case MsgType::ConfirmConfig:
                // Wait for IpAddrPort before actually adding this new spectator
                return;

            case MsgType::IpAddrPort:
                if ( socket == dataSocket.get() || !isPendingSocket ( socket ) )
                    break;

                pushSpectator ( socket, { socket->address.addr, msg->getAs<IpAddrPort>().port } );
                return;

            case MsgType::RngState:
                netMan.setRngState ( msg->getAs<RngState>() );
                return;

#ifndef RELEASE
            case MsgType::SyncHash:
                remoteSync.push_back ( msg );
                return;
#endif // NOT RELEASE

            default:
                break;
        }

        switch ( clientMode.value )
        {
            case ClientMode::Host:
                if ( msg->getMsgType() == MsgType::IpAddrPort && socket == dataSocket.get() )
                {
                    clientServerAddr = msg->getAs<IpAddrPort>();
                    clientServerAddr.addr = dataSocket->address.addr;
                    clientServerAddr.invalidate();
                    return;
                }

            case ClientMode::Client:
                switch ( msg->getMsgType() )
                {
                    case MsgType::PlayerInputs:
                        netMan.setInputs ( remotePlayer, msg->getAs<PlayerInputs>() );
                        return;

                    case MsgType::MenuIndex:
                        netMan.setRemoteRetryMenuIndex ( msg->getAs<MenuIndex>().menuIndex );
                        return;

                    // We now ignore remote ChangeConfigs, since delay/rollback is now set independently
                    // case MsgType::ChangeConfig:
                    //     // Only use the ChangeConfig if it is for a later frame than the current ChangeConfig.
                    //     // If for the same frame, then the host's ChangeConfig always takes priority.
                    //     if ( ( msg->getAs<ChangeConfig>().indexedFrame.value > changeConfig.indexedFrame.value )
                    //             || ( msg->getAs<ChangeConfig>().indexedFrame.value == changeConfig.indexedFrame.value
                    //                  && clientMode.isClient() ) )
                    //     {
                    //         shouldChangeDelayRollback = true;
                    //         changeConfig = msg->getAs<ChangeConfig>();
                    //     }
                    //     return;

                    case MsgType::TransitionIndex:
                        netMan.setRemoteIndex ( msg->getAs<TransitionIndex>().index );
                        return;

                    case MsgType::ErrorMessage:
                        if ( lazyDisconnect )
                            return;

                        delayedStop ( msg->getAs<ErrorMessage>().error );
                        return;

                    default:
                        break;
                }
                break;

            case ClientMode::SpectateNetplay:
            case ClientMode::SpectateBroadcast:
                switch ( msg->getMsgType() )
                {
                    case MsgType::InitialGameState:
                        netMan.initial = msg->getAs<InitialGameState>();

                        if ( netMan.initial.chara[0] == UNKNOWN_POSITION )
                            THROW_EXCEPTION ( "chara[0]=Unknown", ERROR_INVALID_HOST_CONFIG );

                        if ( netMan.initial.chara[1] == UNKNOWN_POSITION )
                            THROW_EXCEPTION ( "chara[1]=Unknown", ERROR_INVALID_HOST_CONFIG );

                        if ( netMan.initial.moon[0] == UNKNOWN_POSITION )
                            THROW_EXCEPTION ( "moon[0]=Unknown", ERROR_INVALID_HOST_CONFIG );

                        if ( netMan.initial.moon[1] == UNKNOWN_POSITION )
                            THROW_EXCEPTION ( "moon[1]=Unknown", ERROR_INVALID_HOST_CONFIG );

                        LOG ( "InitialGameState: %s; indexedFrame=[%s]; stage=%u; isTraining=%u; %s vs %s",
                              NetplayState ( ( NetplayState::Enum ) netMan.initial.netplayState ),
                              netMan.initial.indexedFrame, netMan.initial.stage, netMan.initial.isTraining,
                              netMan.initial.formatCharaName ( 1, getFullCharaName ),
                              netMan.initial.formatCharaName ( 2, getFullCharaName ) );

                        netplayStateChanged ( NetplayState::Initial );
                        return;

                    case MsgType::BothInputs:
                        netMan.setBothInputs ( msg->getAs<BothInputs>() );
                        return;

                    case MsgType::MenuIndex:
                        netMan.setRetryMenuIndex ( msg->getAs<MenuIndex>().index, msg->getAs<MenuIndex>().menuIndex );
                        return;

                    case MsgType::ErrorMessage:
                        delayedStop ( msg->getAs<ErrorMessage>().error );
                        return;

                    default:
                        break;
                }
                break;

            default:
                break;
        }

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    // ProcessManager callbacks
    void ipcConnectEvent() override
    {
    }

    void ipcDisconnectEvent() override
    {
        appState = AppState::Stopping;
        EventManager::get().stop();
        stopping = true;
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::OptionsMessage:
                options = msg->getAs<OptionsMessage>();

                ProcessManager::appDir = options.arg ( Options::AppDir );

                // This will log in the previous appDir folder it not the same
                LOG ( "appDir='%s'", ProcessManager::appDir );

                Logger::get().sessionId = options.arg ( Options::SessionId );
                Logger::get().initialize ( ProcessManager::appDir + LOG_FILE );
                Logger::get().logVersion();

                LOG ( "gameDir='%s'", ProcessManager::gameDir );
                LOG ( "appDir='%s'", ProcessManager::appDir );

                syncLog.sessionId = options.arg ( Options::SessionId );
                syncLog.initialize ( ProcessManager::appDir + SYNC_LOG_FILE, 0 );
                syncLog.logVersion();

                // Manually hit Alt+Enter to enable fullscreen
                if ( options[Options::Fullscreen] && DllHacks::windowHandle == GetForegroundWindow() )
                {
                    INPUT inputs[4];

                    for ( INPUT& input : inputs )
                    {
                        input.type = INPUT_KEYBOARD;
                        input.ki.time = 0;
                        input.ki.dwExtraInfo = 0;
                        input.ki.dwFlags = KEYEVENTF_SCANCODE;
                    }

                    inputs[0].ki.wScan = MapVirtualKey ( VK_MENU, MAPVK_VK_TO_VSC );
                    inputs[0].ki.wVk = VK_MENU;

                    inputs[1].ki.wScan = MapVirtualKey ( VK_RETURN, MAPVK_VK_TO_VSC );
                    inputs[1].ki.wVk = VK_RETURN;

                    inputs[2].ki.wScan = MapVirtualKey ( VK_RETURN, MAPVK_VK_TO_VSC );
                    inputs[2].ki.wVk = VK_RETURN;
                    inputs[2].ki.dwFlags |= KEYEVENTF_KEYUP;

                    inputs[3].ki.wScan = MapVirtualKey ( VK_MENU, MAPVK_VK_TO_VSC );
                    inputs[3].ki.wVk = VK_MENU;
                    inputs[3].ki.dwFlags |= KEYEVENTF_KEYUP;

                    SendInput ( 4, inputs, sizeof ( INPUT ) );
                }

#ifndef RELEASE
                if ( options[Options::Replay] )
                {
                    LOG ( "Replay: '%s'", options.arg ( Options::Replay ) );

                    // Parse arguments
                    const vector<string> args = split ( options.arg ( Options::Replay ), "," );
                    ASSERT ( args.empty() == false );

                    const string replayFile = ProcessManager::appDir + args[0];
                    const bool real = find ( args.begin(), args.end(), "real" ) != args.end();

                    // Parse replay speed
                    auto it = find ( args.begin(), args.end(), "speed" );
                    if ( it != args.end() )
                        ++it;
                    if ( it != args.end() )
                        replaySpeed = lexical_cast<uint32_t> ( *it );

                    // Parse stop index and frame
                    it = find ( args.begin(), args.end(), "stop" );
                    if ( it != args.end() )
                        ++it;
                    if ( it != args.end() && ( args.end() - it ) >= 2 )
                    {
                        replayStop.parts.index = lexical_cast<uint32_t> ( *it++ );
                        replayStop.parts.frame = lexical_cast<uint32_t> ( *it++ );
                    }

                    // Parse check RngState args
                    it = find ( args.begin(), args.end(), "check" );
                    if ( it != args.end() )
                        ++it;
                    if ( it != args.end() && ( args.end() - it ) >= 3 )
                    {
                        replayCheck.parts.index = lexical_cast<uint32_t> ( *it++ );
                        replayCheck.parts.frame = lexical_cast<uint32_t> ( *it++ );
                        replayCheckRngHexStr = ( *it++ );
                    }

                    // Parse replay file
                    const bool good = repMan.load ( replayFile, real );
                    ASSERT ( good == true );

                    // Parse start index
                    it = find ( args.begin(), args.end(), "start" );
                    if ( it != args.end() )
                        ++it;
                    if ( it != args.end() )
                    {
                        MsgPtr msgInitialState = repMan.getInitialStateBefore ( lexical_cast<int> ( *it ) );

                        ASSERT ( msgInitialState.get() != 0 );

                        netMan.initial = msgInitialState->getAs<InitialGameState>();
                        netMan.initial.netplayState = 0xFF;
                        netMan.initial.stage = 1;
                    }

                    replayInputs = true;
                }
                else
                {
                    randomInputs = options[Options::SyncTest];
                }
#endif // NOT RELEASE
                break;

            case MsgType::ControllerMappings:
                KeyboardState::clear();
                initControllers ( msg->getAs<ControllerMappings>() );
                break;

            case MsgType::ClientMode:
                if ( clientMode != ClientMode::Unknown )
                    break;

                clientMode = msg->getAs<ClientMode>();
                clientMode.flags |= ClientMode::GameStarted;

                if ( clientMode.isTraining() )
                    WRITE_ASM_HACK ( AsmHacks::forceGotoTraining );
                else if ( clientMode.isVersusCpu() )
                    WRITE_ASM_HACK ( AsmHacks::forceGotoVersusCpu );
                else
                    WRITE_ASM_HACK ( AsmHacks::forceGotoVersus );

                isSinglePlayer = clientMode.isSinglePlayer();

                LOG ( "%s: flags={ %s }", clientMode, clientMode.flagString() );
                break;

            case MsgType::IpAddrPort:
                if ( !address.empty() )
                    break;

                address = msg->getAs<IpAddrPort>();
                LOG ( "address='%s'", address );
                break;

            case MsgType::SpectateConfig:
                ASSERT ( clientMode.isSpectate() == true );

                netMan.config.mode       = clientMode;
                netMan.config.mode.flags |= msg->getAs<SpectateConfig>().mode.flags;
                netMan.config.sessionId  = Logger::get().sessionId;
                netMan.config.delay      = msg->getAs<SpectateConfig>().delay;
                netMan.config.rollback   = msg->getAs<SpectateConfig>().rollback;
                netMan.config.winCount   = msg->getAs<SpectateConfig>().winCount;
                netMan.config.hostPlayer = msg->getAs<SpectateConfig>().hostPlayer;
                netMan.config.names      = msg->getAs<SpectateConfig>().names;
                netMan.config.sessionId  = msg->getAs<SpectateConfig>().sessionId;

                if ( netMan.config.delay == 0xFF )
                    THROW_EXCEPTION ( "delay=%u", ERROR_INVALID_HOST_CONFIG, netMan.config.delay );

                netMan.initial = msg->getAs<SpectateConfig>().initial;

                if ( netMan.initial.netplayState == NetplayState::Unknown )
                    THROW_EXCEPTION ( "netplayState=NetplayState::Unknown", ERROR_INVALID_HOST_CONFIG );

                if ( netMan.initial.chara[0] == UNKNOWN_POSITION )
                    THROW_EXCEPTION ( "chara[0]=Unknown", ERROR_INVALID_HOST_CONFIG );

                if ( netMan.initial.chara[1] == UNKNOWN_POSITION )
                    THROW_EXCEPTION ( "chara[1]=Unknown", ERROR_INVALID_HOST_CONFIG );

                if ( netMan.initial.moon[0] == UNKNOWN_POSITION )
                    THROW_EXCEPTION ( "moon[0]=Unknown", ERROR_INVALID_HOST_CONFIG );

                if ( netMan.initial.moon[1] == UNKNOWN_POSITION )
                    THROW_EXCEPTION ( "moon[1]=Unknown", ERROR_INVALID_HOST_CONFIG );

                LOG ( "SpectateConfig: %s; flags={ %s }; delay=%d; rollback=%d; winCount=%d; hostPlayer=%u; "
                      "names={ '%s', '%s' }", netMan.config.mode, netMan.config.mode.flagString(), netMan.config.delay,
                      netMan.config.rollback, netMan.config.winCount, netMan.config.hostPlayer,
                      netMan.config.names[0], netMan.config.names[1] );

                LOG ( "InitialGameState: %s; stage=%u; isTraining=%u; %s vs %s",
                      NetplayState ( ( NetplayState::Enum ) netMan.initial.netplayState ),
                      netMan.initial.stage, netMan.initial.isTraining,
                      msg->getAs<SpectateConfig>().formatPlayer ( 1, getFullCharaName ),
                      msg->getAs<SpectateConfig>().formatPlayer ( 2, getFullCharaName ) );

                serverCtrlSocket = SmartSocket::listenTCP ( this, 0 );
                LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );

                procMan.ipcSend ( serverCtrlSocket->address );

                *CC_DAMAGE_LEVEL_ADDR = 2;
                *CC_TIMER_SPEED_ADDR = 2;
                *CC_WIN_COUNT_VS_ADDR = ( uint32_t ) ( netMan.config.winCount ? netMan.config.winCount : 2 );

                // *CC_WIN_COUNT_VS_ADDR = 1;
                // *CC_DAMAGE_LEVEL_ADDR = 4;

                // Wait for final InitialGameState message before going to NetplayState::Initial
                break;

            case MsgType::NetplayConfig:
                if ( netMan.config.delay != 0xFF )
                    break;

                netMan.config = msg->getAs<NetplayConfig>();
                netMan.config.mode = clientMode;
                netMan.config.sessionId = Logger::get().sessionId;

                if ( netMan.config.delay == 0xFF )
                    THROW_EXCEPTION ( "delay=%u", ERROR_INVALID_HOST_CONFIG, netMan.config.delay );

                if ( clientMode.isNetplay() )
                {
                    if ( netMan.config.hostPlayer != 1 && netMan.config.hostPlayer != 2 )
                        THROW_EXCEPTION ( "hostPlayer=%u", ERROR_INVALID_HOST_CONFIG, netMan.config.hostPlayer );

                    // Determine the player numbers
                    if ( clientMode.isHost() )
                    {
                        localPlayer = netMan.config.hostPlayer;
                        remotePlayer = ( 3 - netMan.config.hostPlayer );
                    }
                    else
                    {
                        remotePlayer = netMan.config.hostPlayer;
                        localPlayer = ( 3 - netMan.config.hostPlayer );
                    }

                    netMan.setRemotePlayer ( remotePlayer );

                    if ( clientMode.isHost() )
                    {
                        serverCtrlSocket = SmartSocket::listenTCP ( this, address.port );
                        LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );

                        serverDataSocket = SmartSocket::listenUDP ( this, address.port );
                        LOG ( "serverDataSocket=%08x", serverDataSocket.get() );
                    }
                    else if ( clientMode.isClient() )
                    {
                        serverCtrlSocket = SmartSocket::listenTCP ( this, 0 );
                        LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );

                        dataSocket = SmartSocket::connectUDP ( this, address, clientMode.isUdpTunnel() );
                        LOG ( "dataSocket=%08x", dataSocket.get() );
                    }

                    initialTimer.reset ( new Timer ( this ) );
                    initialTimer->start ( INITIAL_CONNECT_TIMEOUT );

                    // Wait for dataSocket to be connected before changing to NetplayState::Initial
                }
                else if ( clientMode.isBroadcast() )
                {
                    ASSERT ( netMan.config.mode.isBroadcast() == true );

                    LOG ( "NetplayConfig: broadcastPort=%u", netMan.config.broadcastPort );

                    serverCtrlSocket = SmartSocket::listenTCP ( this, netMan.config.broadcastPort );
                    LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );

                    // Update the broadcast port and send over IPC
                    netMan.config.broadcastPort = serverCtrlSocket->address.port;
                    netMan.config.invalidate();

                    procMan.ipcSend ( netMan.config );

                    netplayStateChanged ( NetplayState::Initial );
                }
                else if ( clientMode.isOffline() )
                {
                    ASSERT ( netMan.config.hostPlayer == 1 || netMan.config.hostPlayer == 2 );

                    localPlayer = netMan.config.hostPlayer;
                    remotePlayer = ( 3 - netMan.config.hostPlayer );

                    netMan.setRemotePlayer ( remotePlayer );

                    netplayStateChanged ( NetplayState::Initial );
                }

                minRollbackSpacing = clamped<uint8_t> ( netMan.config.rollback, 2, 4 );
                rollbackTimer = minRollbackSpacing;

                *CC_DAMAGE_LEVEL_ADDR = 2;
                *CC_TIMER_SPEED_ADDR = 2;
                *CC_WIN_COUNT_VS_ADDR = ( uint32_t ) ( netMan.config.winCount ? netMan.config.winCount : 2 );

                // *CC_WIN_COUNT_VS_ADDR = 1;
                // *CC_DAMAGE_LEVEL_ADDR = 4;

                // Rollback specific game hacks
                if ( netMan.getRollback() )
                {
                    // Manually control intro state
                    WRITE_ASM_HACK ( AsmHacks::hijackIntroState );

                    // Disable auto replay save (TODO)
                    *CC_AUTO_REPLAY_SAVE_ADDR = 0;

                    // Disable stage animations (TODO)
                    *CC_STAGE_ANIMATION_OFF_ADDR = 1;
                }

                LOG ( "SessionId '%s'", netMan.config.sessionId );

                LOG ( "NetplayConfig: %s; flags={ %s }; delay=%d; rollback=%d; rollbackDelay=%d; winCount=%d; "
                      "hostPlayer=%d; localPlayer=%d; remotePlayer=%d; names={ '%s', '%s' }",
                      netMan.config.mode, netMan.config.mode.flagString(), netMan.config.delay, netMan.config.rollback,
                      netMan.config.rollbackDelay, netMan.config.winCount, netMan.config.hostPlayer,
                      localPlayer, remotePlayer, netMan.config.names[0], netMan.config.names[1] );
                break;

            default:
                if ( clientMode.isSpectate() )
                {
                    readEvent ( 0, msg, NullAddress );
                    break;
                }

                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
        if ( timer == resendTimer.get() )
        {
            dataSocket->send ( netMan.getInputs ( localPlayer ) );
            resendTimer->start ( RESEND_INPUTS_INTERVAL );

            ++waitInputsTimer;

            if ( waitInputsTimer > ( MAX_WAIT_INPUTS_INTERVAL / RESEND_INPUTS_INTERVAL ) )
                delayedStop ( "Timed out!" );
        }
        else if ( timer == initialTimer.get() )
        {
            delayedStop ( "Disconnected!" );
            initialTimer.reset();
        }
        else if ( timer == stopTimer.get() )
        {
            appState = AppState::Stopping;
            EventManager::get().stop();
            stopping = true;
        }
        else
        {
            SpectatorManager::timerExpired ( timer );
        }
    }

    // DLL callback
    void callback()
    {
        // Check if the game is being closed
        if ( ! ( * CC_ALIVE_FLAG_ADDR ) )
        {
            // Disconnect the main data socket if netplay
            if ( clientMode.isNetplay() && dataSocket )
                dataSocket->disconnect();

            // Disconnect all other sockets
            if ( ctrlSocket )
                ctrlSocket->disconnect();

            if ( serverCtrlSocket )
                serverCtrlSocket->disconnect();

            appState = AppState::Stopping;
            EventManager::get().stop();
            stopping = true;
        }

        // Don't poll unless we're in the correct state
        if ( appState != AppState::Polling )
            return;

        // Check if the world timer changed, this calls hasChanged if changed, which calls frameStep
        worldTimerMoniter.check();
    }

    // Constructor
    DllMain()
        : SpectatorManager ( &netMan, &procMan )
        , worldTimerMoniter ( this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR )
    {
        // Timer and controller initialization is not done here because of threading issues

        procMan.connectPipe();

        netplayStateChanged ( NetplayState::PreInitial );

        ChangeMonitor::get().addRef ( this, Variable ( Variable::GameMode ), *CC_GAME_MODE_ADDR );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::RoundStart ), AsmHacks::roundStartCounter );

#ifndef RELEASE
        ChangeMonitor::get().addRef ( this, Variable ( Variable::MenuConfirmState ), AsmHacks::menuConfirmState );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::CurrentMenuIndex ), AsmHacks::currentMenuIndex );
        // ChangeMonitor::get().addRef ( this, Variable ( Variable::GameStateCounter ), *CC_MENU_STATE_COUNTER_ADDR );
        // ChangeMonitor::get().addPtrToRef ( this, Variable ( Variable::AutoReplaySave ),
        //                                    const_cast<const uint32_t *&> ( AsmHacks::autoReplaySaveStatePtr ), 0u );
#endif // NOT RELEASE
    }

    // Destructor
    ~DllMain()
    {
        rollMan.deallocateStates();

        KeyboardManager::get().unhook();

        syncLog.deinitialize();

        procMan.disconnectPipe();

        ControllerManager::get().owner = 0;

        // Timer and controller deinitialization is not done here because of threading issues
    }

private:

    void saveMappings ( const Controller *controller ) const override
    {
        if ( !controller )
            return;

        const string file = ProcessManager::appDir + FOLDER + controller->getName() + MAPPINGS_EXT;

        LOG ( "Saving: %s", file );

        if ( controller->saveMappings ( file ) )
            return;

        LOG ( "Failed to save: %s", file );
    }

    const IpAddrPort& getRandomRedirectAddress() const
    {
        size_t r = rand() % ( 1 + numSpectators() );

        if ( r == 0 && !clientServerAddr.empty() )
            return clientServerAddr;
        else
            return getRandomSpectatorAddress();
    }
};


static void initializeDllMain()
{
    main.reset ( new DllMain() );
}

static void deinitialize()
{
    LOCK ( deinitMutex );

    if ( appState == AppState::Deinitialized )
        return;

    main.reset();

    EventManager::get().release();
    TimerManager::get().deinitialize();
    SocketManager::get().deinitialize();
    // Joystick must be deinitialized on the same thread it was initialized, ie not here
    Logger::get().deinitialize();

    DllHacks::deinitialize();

    appState = AppState::Deinitialized;
}

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
        {
            char buffer[4096];
            string gameDir;

            if ( GetModuleFileName ( GetModuleHandle ( 0 ), buffer, sizeof ( buffer ) ) )
            {
                gameDir = buffer;
                gameDir = gameDir.substr ( 0, gameDir.find_last_of ( "/\\" ) );

                replace ( gameDir.begin(), gameDir.end(), '/', '\\' );

                if ( !gameDir.empty() && gameDir.back() != '\\' )
                    gameDir += '\\';
            }

            ProcessManager::gameDir = gameDir;


            srand ( time ( 0 ) );

            Logger::get().initialize ( gameDir + LOG_FILE );
            Logger::get().logVersion();

            LOG ( "DLL_PROCESS_ATTACH" );
            LOG ( "gameDir='%s'", gameDir );

            // We want the DLL to be able to rebind any previously bound ports
            Socket::forceReusePort ( true );

            try
            {
                // It is safe to initialize sockets here
                SocketManager::get().initialize();
                DllHacks::initializePreLoad();
                initializeDllMain();
            }
            catch ( const Exception& exc )
            {
                exit ( -1 );
            }
#ifdef NDEBUG
            catch ( const std::exception& exc )
            {
                exit ( -1 );
            }
            catch ( ... )
            {
                exit ( -1 );
            }
#endif // NDEBUG
            break;
        }

        case DLL_PROCESS_DETACH:
            LOG ( "DLL_PROCESS_DETACH" );
            appState = AppState::Stopping;
            EventManager::get().release();
            exit ( 0 );
            break;
    }

    return TRUE;
}


void stopDllMain ( const string& error )
{
    if ( main )
    {
        main->delayedStop ( error );
    }
    else
    {
        appState = AppState::Stopping;
        EventManager::get().stop();
    }

    stopping = true;
}

namespace AsmHacks
{

extern "C" void callback()
{
    if ( appState == AppState::Deinitialized )
        return;

    try
    {
        if ( appState == AppState::Uninitialized )
        {
            DllHacks::initializePostLoad();
            KeyboardState::windowHandle = DllHacks::windowHandle;

            // Joystick and timer must be initialized in the main thread
            TimerManager::get().initialize();
            ControllerManager::get().windowHandle = DllHacks::windowHandle;
            ControllerManager::get().initialize ( 0 );

            // Start polling now
            EventManager::get().startPolling();
            appState = AppState::Polling;
        }

        ASSERT ( main.get() != 0 );

        main->callback();
    }
    catch ( const Exception& exc )
    {
        LOG ( "Stopping due to exception: %s", exc );
        stopDllMain ( exc.user );
    }
#ifdef NDEBUG
    catch ( const std::exception& exc )
    {
        LOG ( "Stopping due to std::exception: %s", exc.what() );
        stopDllMain ( string ( "Error: " ) + exc.what() );
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );
        stopDllMain ( "Unknown error!" );
    }
#endif // NDEBUG

    if ( appState == AppState::Stopping )
    {
        LOG ( "Exiting" );

        // Joystick must be deinitialized on the main thread it was initialized
        ControllerManager::get().deinitialize();
        deinitialize();

        *CC_ALIVE_FLAG_ADDR = 0;
    }
}

} // namespace AsmHacks
