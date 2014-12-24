#include "Main.h"
#include "Thread.h"
#include "DllHacks.h"
#include "DllOverlayUi.h"
#include "NetplayManager.h"
#include "ChangeMonitor.h"
#include "SmartSocket.h"
#include "UdpSocket.h"
#include "Exceptions.h"
#include "Enum.h"
#include "ErrorStringsExt.h"
#include "KeyboardState.h"
#include "CharacterSelect.h"
#include "SpectatorManager.h"
#include "DllControllerManager.h"

#include <windows.h>

#include <vector>
#include <memory>
#include <algorithm>

using namespace std;


// The main log file path
#define LOG_FILE                    FOLDER "dll.log"

// The number of milliseconds to wait to perform a delayed stop so that ErrorMessages are received in time
#define DELAYED_STOP                ( 100 )

// The number of milliseconds before resending inputs while waiting for more inputs
#define RESEND_INPUTS_INTERVAL      ( 100 )

// The maximum number of spectators allowed for ClientMode::Spectate
#define MAX_SPECTATORS              ( 15 )

// The maximum number of spectators allowed for ClientMode::Host/Client
#define MAX_ROOT_SPECTATORS         ( 1 )

// Indicates if this client should redirect spectators
#define SHOULD_REDIRECT_SPECTATORS  ( clientMode.isSpectate()                                                   \
                                      ? numSpectators() >= MAX_SPECTATORS                                       \
                                      : numSpectators() >= MAX_ROOT_SPECTATORS )


#define LOG_SYNC(FORMAT, ...)                                                                                   \
    LOG_TO ( syncLog, "%s [%u] %s [%s] " FORMAT,                                                                \
             gameModeStr ( *CC_GAME_MODE_ADDR ), *CC_GAME_MODE_ADDR,                                            \
             netMan.getState(), netMan.getIndexedFrame(), __VA_ARGS__ )


// Main application state
static ENUM ( AppState, Uninitialized, Polling, Stopping, Deinitialized ) appState = AppState::Uninitialized;

// Main application instance
struct DllMain;
static shared_ptr<DllMain> main;

// Mutex for deinitialize()
static Mutex deinitMutex;
static void deinitialize();

// Enum of variables to monitor
ENUM ( Variable, WorldTime, GameMode, RoundStart, SkippableFlag,
       MenuConfirmState, AutoReplaySave, GameStateCounter, CurrentMenuIndex );


struct DllMain
        : public Main
        , public RefChangeMonitor<Variable, uint32_t>::Owner
        , public PtrToRefChangeMonitor<Variable, uint32_t>::Owner
        , public SpectatorManager
        , public DllControllerManager
{
    // NetplayManager instance
    NetplayManager netMan;

    // If remote has loaded up to character select
    bool remoteCharaSelectLoaded = false;

    // ChangeMonitor for CC_WORLD_TIMER_ADDR
    RefChangeMonitor<Variable, uint32_t> worldTimerMoniter;

    // Timeout for each call to EventManager::poll
    // TODO figure out if there is any way to increase this, maybe dynamically?
    uint64_t pollTimeout = 1;

    // Timer for resending inputs while waiting
    TimerPtr resendTimer;

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

    // If the game is over now (after checking P1 and P2 health values)
    bool isGameOver = false;

    // If the delay and/or rollback should be changed
    bool shouldChangeDelayRollback = false;

    // Latest ChangeConfig for changing delay/rollback
    ChangeConfig changeConfig;

    // Client serverCtrlSocket address
    IpAddrPort clientServerAddr;

    // Sockets that have been redirected to another client
    unordered_set<Socket *> redirectedSockets;

#ifndef RELEASE
    // Local and remote SyncHashes
    list<MsgPtr> localSync, remoteSync;
#endif


    void frameStepNormal()
    {
        switch ( netMan.getState().value )
        {
            case NetplayState::PreInitial:
            case NetplayState::Initial:
            case NetplayState::AutoCharaSelect:
                // Disable FPS limit while going to character select
                *CC_SKIP_FRAMES_ADDR = 1;
                break;

            case NetplayState::InGame:
                // Only save rollback states in-game
                if ( netMan.isRollbackState() )
                    procMan.saveState ( netMan );

            case NetplayState::CharaSelect:
            case NetplayState::Loading:
            case NetplayState::Skippable:
            case NetplayState::RetryMenu:
            {
                // Fast forward if spectator
                if ( clientMode.isSpectate() && netMan.getState() != NetplayState::Loading )
                {
                    static bool doneSkipping = true;

                    const IndexedFrame remoteIndexedFrame = netMan.getRemoteIndexedFrame();

                    if ( doneSkipping && remoteIndexedFrame.value > netMan.getIndexedFrame().value + NUM_INPUTS )
                    {
                        *CC_SKIP_FRAMES_ADDR = 1;
                        doneSkipping = false;
                    }
                    else if ( !doneSkipping && *CC_SKIP_FRAMES_ADDR == 0 )
                    {
                        doneSkipping = true;
                    }
                }

                ASSERT ( localPlayer == 1 || localPlayer == 2 );

                checkOverlay ( netMan.getState().value == NetplayState::CharaSelect || clientMode.isNetplay() );

                KeyboardState::update();
                ControllerManager::get().check();

                if ( DllOverlayUi::isEnabled() )                // Overlay UI input
                {
                    localInputs[0] = localInputs[1] = 0;
                }
                else if ( clientMode.isNetplay() )              // Netplay input
                {
                    if ( playerControllers[localPlayer - 1] )
                        localInputs[0] = getInput ( playerControllers[localPlayer - 1] );

                    if ( GetKeyState ( VK_CONTROL ) & 0x80 )
                    {
                        for ( uint8_t delay = 0; delay < 10; ++delay )
                        {
                            if ( delay == netMan.getDelay() )
                                continue;

                            if ( ( GetKeyState ( '0' + delay ) & 0x80 )                 // Ctrl + Number
                                    || ( GetKeyState ( VK_NUMPAD0 + delay ) & 0x80 ) )  // Ctrl + Numpad Number
                            {
                                shouldChangeDelayRollback = true;
                                changeConfig.indexedFrame = netMan.getIndexedFrame();
                                changeConfig.delay = delay;
                                dataSocket->send ( changeConfig );
                                break;
                            }
                        }
                    }

                    // TODO Alt+Number to change rollback

#ifndef RELEASE
                    // Test random delay setting
                    static bool randomize = false;

                    if ( KeyboardState::isPressed ( VK_F11 ) )
                        randomize = !randomize;

                    if ( randomize && ( rand() % 30 ) == ( clientMode.isHost() ? 0 : 15 ) )
                    {
                        shouldChangeDelayRollback = true;
                        changeConfig.indexedFrame = netMan.getIndexedFrame();
                        changeConfig.delay = rand() % 10;
                        dataSocket->send ( changeConfig );
                    }
#endif
                }
                else if ( clientMode.isLocal() )                // Local input
                {
                    if ( playerControllers[localPlayer - 1] )
                        localInputs[0] = getInput ( playerControllers[localPlayer - 1] );
                }
                else if ( clientMode.isSpectate() )             // Spectator input
                {
                    if ( KeyboardState::isDown ( VK_SPACE ) )
                        *CC_SKIP_FRAMES_ADDR = 0;
                }
                else
                {
                    LOG ( "Unknown clientMode=%s; flags={ %s }", clientMode, clientMode.flagString() );
                    break;
                }

#ifndef RELEASE
                // Test random input
                static bool randomize = false;

                if ( KeyboardState::isPressed ( VK_F12 ) )
                    randomize = !randomize;

                if ( randomize )
                {
                    if ( netMan.getFrame() % 2 )
                    {
                        uint16_t direction = ( rand() % 10 );

                        // Reduce the chances of moving the cursor at retry menu
                        if ( netMan.getState().value == NetplayState::RetryMenu && ( rand() % 2 ) )
                            direction = 0;

                        uint16_t buttons = ( rand() % 0x1000 );

                        // Prevent hitting some non-essential buttons
                        buttons &= ~ ( CC_BUTTON_D | CC_BUTTON_FN1 | CC_BUTTON_FN2 | CC_BUTTON_START );

                        // Prevent going back at character select
                        if ( netMan.getState().value == NetplayState::CharaSelect )
                            buttons &= ~ ( CC_BUTTON_B | CC_BUTTON_CANCEL );

                        if ( clientMode.isLocal() )
                            localInputs[1] = COMBINE_INPUT ( direction, buttons );
                        else
                            localInputs[0] = COMBINE_INPUT ( direction, buttons );
                    }
                }
#endif

                // Assign local player input
                if ( !clientMode.isSpectate() )
                    netMan.setInput ( localPlayer, localInputs[0] );

                if ( clientMode.isNetplay() )
                {
                    // Special netplay retry menu behaviour, only select final option after both sides have selected
                    if ( netMan.getState().value == NetplayState::RetryMenu )
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
                    if ( playerControllers[1] && !DllOverlayUi::isEnabled() )
                        localInputs[1] = getInput ( playerControllers[1] );

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

        // // Clear the last changed frame before we get new inputs
        // netMan.clearLastChangedFrame();

        for ( ;; )
        {
            // Poll until we are ready to run
            if ( !EventManager::get().poll ( pollTimeout ) )
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
                // Continue if ready or isGameOver
                if ( ready || isGameOver )
                    break;
            }
            else
            {
                // Stop resending inputs if we're ready
                if ( ready )
                {
                    resendTimer.reset();
                    break;
                }

                // Start resending inputs since we are waiting
                if ( !resendTimer )
                {
                    resendTimer.reset ( new Timer ( this ) );
                    resendTimer->start ( RESEND_INPUTS_INTERVAL );
                }
            }
        }

        // // Only do rollback related stuff while in-game
        // if ( netMan.getState() == NetplayState::InGame && netMan.isRollbackState()
        //         && netMan.getLastChangedFrame().value < netMan.getIndexedFrame().value )
        // {
        //     LOG_SYNC ( "Rollback: %s -> %s", netMan.getIndexedFrame(), netMan.getLastChangedFrame() );

        //     // Indicate we're re-running (save the frame first)
        //     fastFwdStopFrame = netMan.getIndexedFrame();

        //     // Reset the game state (this resets game state and netMan state)
        //     procMan.loadState ( netMan.getLastChangedFrame(), netMan );
        //     return;
        // }

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

            if ( changeConfig.delay != 0xFF )
            {
                netMan.setDelay ( changeConfig.delay );
                // netMan.setRollback ( changeConfig.rollback );
            }
        }

#ifndef RELEASE
        // Log the RngState once every 5 seconds after CharaSelect, except in Loading, Skippable, and RetryMenu states.
        // This effectively also logs whenever the frame becomes zero, ie when the index is incremented.
        if ( dataSocket && dataSocket->isConnected() && netMan.getFrame() % ( 5 * 60 ) == 0
                && netMan.getState().value >= NetplayState::CharaSelect && netMan.getState() != NetplayState::Loading
                && netMan.getState() != NetplayState::Skippable && netMan.getState() != NetplayState::RetryMenu )
        {
            MsgPtr msgRngState = procMan.getRngState ( netMan.getIndex() );

            ASSERT ( msgRngState.get() != 0 );

            LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );

            // Check for desyncs by periodically sending hashes
            MsgPtr msgSyncHash ( new SyncHash ( netMan.getIndexedFrame(), msgRngState->getAs<RngState>() ) );

            dataSocket->send ( msgSyncHash );

            localSync.push_back ( msgSyncHash );

            while ( !localSync.empty() && !remoteSync.empty() )
            {
                if ( localSync.front()->getAs<SyncHash>() == remoteSync.front()->getAs<SyncHash>() )
                {
                    localSync.pop_front();
                    remoteSync.pop_front();
                    continue;
                }

                LOG_SYNC ( "Desync: local=[%s]; remote=[%s]",
                           localSync.front()->getAs<SyncHash>().indexedFrame,
                           remoteSync.front()->getAs<SyncHash>().indexedFrame );

                delayedStop ( "Desync!" );
                return;
            }
        }
#endif

        // Log inputs every frame
        LOG_SYNC ( "Inputs: %04x %04x", netMan.getRawInput ( 1 ), netMan.getRawInput ( 2 ) );
    }

    void frameStepRerun()
    {
        // Stop fast-forwarding once we're reached the frame we want
        if ( netMan.getIndexedFrame().value >= fastFwdStopFrame.value )
            fastFwdStopFrame.value = 0;

        // Disable FPS limit while fast-forwarding
        if ( fastFwdStopFrame.value )
            *CC_SKIP_FRAMES_ADDR = 1;
    }

    void frameStep()
    {
        // New frame
        netMan.updateFrame();
        procMan.clearInputs();

        // Check for changes to important variables for state transitions
        ChangeMonitor::get().check();

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
    }

    void netplayStateChanged ( NetplayState state )
    {
        ASSERT ( netMan.getState() != state );

        DllOverlayUi::disable();

        // Entering InGame
        if ( state == NetplayState::InGame )
        {
            if ( netMan.isRollbackState() )
                procMan.allocateStates();
        }

        // Exiting InGame
        if ( state != NetplayState::InGame )
        {
            if ( netMan.config.rollback )
                procMan.deallocateStates();
        }

        // Entering CharaSelect OR entering InGame
        if ( !clientMode.isOffline() && ( state == NetplayState::CharaSelect || state == NetplayState::InGame ) )
        {
            shouldSyncRngState = !isGameOver;
            LOG ( "[%s] shouldSyncRngState=%u", netMan.getIndexedFrame(), shouldSyncRngState );
        }

        // Entering RetryMenu (enable lazy disconnect on netplay)
        if ( state == NetplayState::RetryMenu )
        {
            lazyDisconnect = clientMode.isNetplay();

            // Clear state flags
            isGameOver = false;
            wasLastRoundDoubleGamePointDraw = false;
            localRetryMenuIndexSent = false;
        }
        else if ( lazyDisconnect )
        {
            lazyDisconnect = false;

            if ( !dataSocket || !dataSocket->isConnected() )
            {
                delayedStop ( "Disconnected!" );
                return;
            }
        }

        netMan.setState ( state );
    }

    void gameModeChanged ( uint32_t previous, uint32_t current )
    {
        if ( current == 0
                || current == CC_GAME_MODE_STARTUP
                || current == CC_GAME_MODE_OPENING
                || current == CC_GAME_MODE_TITLE
                || current == CC_GAME_MODE_MAIN
                || current == CC_GAME_MODE_LOADING_DEMO
                || ( previous == CC_GAME_MODE_LOADING_DEMO && current == CC_GAME_MODE_INGAME )
                || current == CC_GAME_MODE_HIGH_SCORES )
        {
            ASSERT ( netMan.getState() == NetplayState::PreInitial || netMan.getState() == NetplayState::Initial );
            return;
        }

        if ( netMan.getState() == NetplayState::Initial && netMan.config.mode.isSpectate()
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

        if ( current == CC_GAME_MODE_INGAME )
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

    void delayedStop ( string error )
    {
        if ( !error.empty() )
            procMan.ipcSend ( new ErrorMessage ( error ) );

        stopTimer.reset ( new Timer ( this ) );
        stopTimer->start ( DELAYED_STOP );
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

            case Variable::SkippableFlag:
                if ( clientMode.isTraining()
                        || ! ( previous == 0 && current == 1 && netMan.getState() == NetplayState::InGame ) )
                    break;

                // Check for game over since we just entered a skippable state
                updateGameOverFlags();

                // When the SkippableFlag is set while InGame (not training mode), we are in a Skippable state
                LOG ( "[%s] %s: previous=%u; current=%u", netMan.getIndexedFrame(), var, previous, current );
                netplayStateChanged ( NetplayState::Skippable );

                // Enable lazy disconnect if someone just won a game (netplay only)
                lazyDisconnect = ( clientMode.isNetplay() && isGameOver );
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

            if ( redirectAddr.empty() )
            {
                newSocket->send ( new VersionConfig ( clientMode ) );
            }
            else
            {
                redirectedSockets.insert ( newSocket.get() );
                newSocket->send ( redirectAddr );
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

        SetForegroundWindow ( ( HWND ) DllHacks::windowHandle );
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
#endif

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
                        if ( netMan.getState() == NetplayState::RetryMenu )
                            netMan.setRemoteRetryMenuIndex ( msg->getAs<MenuIndex>().menuIndex );
                        return;

                    case MsgType::ChangeConfig:
                        if ( ( msg->getAs<ChangeConfig>().indexedFrame.value > changeConfig.indexedFrame.value )
                                || ( msg->getAs<ChangeConfig>().indexedFrame.value == changeConfig.indexedFrame.value
                                     && clientMode.isClient() ) )
                        {
                            shouldChangeDelayRollback = true;
                            changeConfig = msg->getAs<ChangeConfig>();
                        }
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
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::OptionsMessage:
                options = msg->getAs<OptionsMessage>();

                Logger::get().sessionId = options.arg ( Options::SessionId );
                Logger::get().initialize ( options.arg ( Options::AppDir ) + LOG_FILE );
                Logger::get().logVersion();

                LOG ( "SessionId '%s'", Logger::get().sessionId );

                syncLog.sessionId = options.arg ( Options::SessionId );
                syncLog.initialize ( options.arg ( Options::AppDir ) + SYNC_LOG_FILE, 0 );
                syncLog.logVersion();
                break;

            case MsgType::ControllerMappings:
                KeyboardState::clear();
                ControllerManager::get().owner = this;
                ControllerManager::get().getKeyboard()->setMappings ( ProcessManager::fetchKeyboardConfig() );
                ControllerManager::get().setMappings ( msg->getAs<ControllerMappings>() );
                ControllerManager::get().check();
                allControllers = ControllerManager::get().getControllers();
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
                    initialTimer->start ( DEFAULT_PENDING_TIMEOUT );

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

                *CC_DAMAGE_LEVEL_ADDR = 2;
                *CC_TIMER_SPEED_ADDR = 2;
                *CC_WIN_COUNT_VS_ADDR = ( uint32_t ) ( netMan.config.winCount ? netMan.config.winCount : 2 );

                // *CC_WIN_COUNT_VS_ADDR = 1;
                // *CC_DAMAGE_LEVEL_ADDR = 4;

                LOG ( "SessionId '%s'", netMan.config.sessionId );

                LOG ( "NetplayConfig: %s; flags={ %s }; delay=%d; rollback=%d; winCount=%d; "
                      "hostPlayer=%d; localPlayer=%d; remotePlayer=%d; names={ '%s', '%s' }",
                      netMan.config.mode, netMan.config.mode.flagString(), netMan.config.delay, netMan.config.rollback,
                      netMan.config.winCount, netMan.config.hostPlayer, localPlayer, remotePlayer,
                      netMan.config.names[0], netMan.config.names[1] );
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
        }

        // Don't poll unless we're in the correct state
        if ( appState != AppState::Polling )
            return;

        // Check if the world timer changed, this calls hasChanged if changed
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
        ChangeMonitor::get().addRef ( this, Variable ( Variable::SkippableFlag ), *CC_SKIPPABLE_FLAG_ADDR );

#ifndef RELEASE
        ChangeMonitor::get().addRef ( this, Variable ( Variable::MenuConfirmState ), AsmHacks::menuConfirmState );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::CurrentMenuIndex ), AsmHacks::currentMenuIndex );
        // ChangeMonitor::get().addRef ( this, Variable ( Variable::GameStateCounter ), *CC_GAME_STATE_COUNTER_ADDR );
        // ChangeMonitor::get().addPtrToRef ( this, Variable ( Variable::AutoReplaySave ),
        //                                    const_cast<const uint32_t *&> ( AsmHacks::autoReplaySaveStatePtr ), 0u );
#endif
    }

    // Destructor
    ~DllMain()
    {
        KeyboardManager::get().unhook();

        syncLog.deinitialize();

        procMan.disconnectPipe();

        ControllerManager::get().owner = 0;

        // Timer and controller deinitialization is not done here because of threading issues
    }

private:

    bool wasLastRoundDoubleGamePointDraw = false;

    void updateGameOverFlags()
    {
        const bool isKnockOut = ( ( *CC_P1_HEALTH_ADDR ) == 0 ) || ( ( *CC_P2_HEALTH_ADDR ) == 0 );
        const bool isTimeOut = ( ( *CC_ROUND_TIMER_ADDR ) == 0 );

        if ( !isKnockOut && !isTimeOut )
        {
            isGameOver = false;
        }
        else
        {
            const bool isP1GamePoint = ( ( *CC_P1_WINS_ADDR ) + 1 == ( *CC_WIN_COUNT_VS_ADDR ) );
            const bool isP2GamePoint = ( ( *CC_P2_WINS_ADDR ) + 1 == ( *CC_WIN_COUNT_VS_ADDR ) );
            const bool didP1Win = ( ( *CC_P1_HEALTH_ADDR ) > ( *CC_P2_HEALTH_ADDR ) );
            const bool didP2Win = ( ( *CC_P1_HEALTH_ADDR ) < ( *CC_P2_HEALTH_ADDR ) );
            const bool isDraw = ( ( *CC_P1_HEALTH_ADDR ) == ( *CC_P2_HEALTH_ADDR ) );

            if ( ( isP1GamePoint && didP1Win ) || ( isP2GamePoint && didP2Win ) )
                isGameOver = true;
            else if ( isP1GamePoint && isP2GamePoint && isDraw && wasLastRoundDoubleGamePointDraw )
                isGameOver = true;
            else
                isGameOver = false;

            LOG ( "[%s] p1wins=%u; p2wins=%u; p1health=%u; p2health=%u; wasLastRoundDoubleGamePointDraw=%u",
                  netMan.getIndexedFrame(), *CC_P1_WINS_ADDR, *CC_P2_WINS_ADDR, *CC_P1_HEALTH_ADDR, *CC_P2_HEALTH_ADDR,
                  wasLastRoundDoubleGamePointDraw );

            wasLastRoundDoubleGamePointDraw = ( isP1GamePoint && isP2GamePoint && isDraw );
        }

        LOG ( "[%s] isGameOver=%u", netMan.getIndexedFrame(), ( isGameOver ? 1 : 0 ) );
    }

    void saveMappings ( const Controller *controller ) const override
    {
        if ( !controller )
            return;

        const string file = options.arg ( Options::AppDir ) + FOLDER + controller->getName() + MAPPINGS_EXT;

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
#endif
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


static void stopDllMain ( const string& error )
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
            ControllerManager::get().initialize ( 0 );
            ControllerManager::get().windowHandle = DllHacks::windowHandle;

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
#endif

    if ( appState == AppState::Stopping )
    {
        LOG ( "Exiting" );

        // Joystick must be deinitialized on the main thread it was initialized
        ControllerManager::get().deinitialize();
        deinitialize();
        exit ( 0 );
    }
}

} // namespace AsmHacks

