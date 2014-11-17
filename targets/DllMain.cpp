#include "Main.h"
#include "Logger.h"
#include "Utilities.h"
#include "Thread.h"
#include "DllHacks.h"
#include "NetplayManager.h"
#include "ChangeMonitor.h"
#include "SmartSocket.h"

#include <windows.h>

#include <vector>
#include <memory>

using namespace std;


#define LOG_FILE FOLDER "dll.log"

#define DELAYED_STOP ( 100 )

#define RESEND_INPUTS_INTERVAL ( 100 )

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
ENUM ( Variable, WorldTime, GameMode, RoundStart, SkippableFlag );


struct DllMain
        : public Main
        , public RefChangeMonitor<Variable, uint32_t>::Owner
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

    // Local and remote player numbers
    uint8_t localPlayer = 1, remotePlayer = 2;

    // Timer for resending inputs while waiting
    TimerPtr resendTimer;

    // Indicates if we should sync the game RngState on this frame
    bool shouldSyncRngState = false;

    // Frame to stop on, when re-running the game due to rollback.
    // Also used as a flag to indicate re-run mode, 0:0 means not re-running.
    IndexedFrame rerunStopFrame = {{ 0, 0 }};

    // Spectator sockets
    unordered_map<Socket *, SocketPtr> specSockets;

    // Initial connect timer
    TimerPtr initialTimer;

    // Local and remote SyncHashes
    list<MsgPtr> localSync, remoteSync;

    // Controller mappings
    ControllerMappings mappings;

    // Player 1 (local) and 2 (remote) controllers
    Controller *controllers[2] = { 0, 0 };

    // Local and remote inputs
    uint16_t localInputs = 0, remoteInputs = 0;


    void frameStepNormal()
    {
        switch ( netMan.getState().value )
        {
            case NetplayState::PreInitial:
            case NetplayState::Initial:
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
            case NetplayState::PauseMenu:
            {
                // Check for changes to controller state
                ControllerManager::get().check();

                if ( clientMode.isSpectate() )
                {
                    // TODO spectator controls
                    break;
                }

                if ( controllers[0] )
                    localInputs = convertInputState ( controllers[0]->getState() );

#ifndef RELEASE
                // Test rollback
                if ( ( GetKeyState ( VK_F10 ) & 0x80 )
                        && netMan.config.rollback
                        && netMan.getState() == NetplayState::InGame
                        && netMan.getFrame() > 60 )
                {
                    // Sleep ( 1000 );
                    procMan.loadState ( { netMan.getIndex(), netMan.getFrame() - 10 }, netMan );
                }

                // Test random inputs
                static bool randomize = false;

                if ( GetKeyState ( VK_F11 ) & 0x80 ) randomize = true;
                if ( GetKeyState ( VK_F12 ) & 0x80 ) randomize = false;

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

                        localInputs = COMBINE_INPUT ( direction, buttons );
                    }
                }
#endif

                netMan.setInput ( localPlayer, localInputs );

                if ( clientMode.isNetplay() )
                {
                    dataSocket->send ( netMan.getInputs ( localPlayer ) );
                }
                else if ( clientMode.isLocal() )
                {
                    if ( controllers[1] )
                        remoteInputs = convertInputState ( controllers[1]->getState() );

                    netMan.setInput ( remotePlayer, remoteInputs );
                }

                if ( shouldSyncRngState && ( clientMode.isHost() || clientMode.isBroadcast() ) )
                {
                    shouldSyncRngState = false;

                    MsgPtr msgRngState = procMan.getRngState ( netMan.getIndex() );

                    netMan.setRngState ( msgRngState->getAs<RngState>() );

                    if ( clientMode.isHost() )
                        dataSocket->send ( msgRngState );

                    for ( const auto& kv : specSockets )
                        kv.first->send ( msgRngState );
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

            if ( appState == AppState::Stopping )
                return;

            // Don't need to wait for anything in local modes
            if ( clientMode.isLocal() )
                break;

            // Check if we are ready to continue running, ie not waiting on remote input or RngState
            const bool ready = ( netMan.isRemoteInputReady() && netMan.isRngStateReady ( shouldSyncRngState ) );

            // Don't resend inputs in spectator mode
            if ( clientMode.isSpectate() )
            {
                if ( ready )
                    break;

                // Just keep polling if not ready
                continue;
            }

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

        // // Only do rollback related stuff while in-game
        // if ( netMan.getState() == NetplayState::InGame && netMan.isRollbackState()
        //         && netMan.getLastChangedFrame().value < netMan.getIndexedFrame().value )
        // {
        //     LOG_SYNC ( "Rollback: %s -> %s", netMan.getIndexedFrame(), netMan.getLastChangedFrame() );

        //     // Indicate we're re-running (save the frame first)
        //     rerunStopFrame = netMan.getIndexedFrame();

        //     // Reset the game state (this resets game state and netMan state)
        //     procMan.loadState ( netMan.getLastChangedFrame(), netMan );
        //     return;
        // }

        // Update the RngState if necessary
        if ( shouldSyncRngState )
        {
            shouldSyncRngState = false;

            MsgPtr msgRngState = netMan.getRngState();

            ASSERT ( msgRngState.get() != 0 );

            procMan.setRngState ( msgRngState->getAs<RngState>() );
        }

        // Broadcast inputs to spectators once every NUM_INPUTS frames
        if ( !specSockets.empty() && netMan.getFrame() % NUM_INPUTS == NUM_INPUTS - 1 )
        {
            MsgPtr msgBothInputs = netMan.getBothInputs();

            ASSERT ( msgBothInputs.get() != 0 );

            for ( const auto& kv : specSockets )
                kv.first->send ( msgBothInputs );
        }

        // Log the RngState once every 5 seconds.
        // This effectively also logs whenever the frame becomes zero, ie when the index is incremented.
        if ( dataSocket && dataSocket->isConnected() && netMan.getFrame() % ( 5 * 60 ) == 0 )
        {
            MsgPtr msgRngState = procMan.getRngState ( netMan.getIndex() );

            LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );

            // Check for desyncs by periodically sending hashes
            if ( options[Options::CheckSync] )
            {
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

                    appState = AppState::Stopping;
                    return;
                }
            }
        }

        // Log inputs every frame
        LOG_SYNC ( "Inputs: %04x %04x", netMan.getInput ( 1 ), netMan.getInput ( 2 ) );
        overlayText = toString ( "%s", netMan.getIndexedFrame() );
    }

    void frameStepRerun()
    {
        // Stop re-running once we're reached the frame we want
        if ( netMan.getIndexedFrame().value >= rerunStopFrame.value )
            rerunStopFrame.value = 0;
    }

    void frameStep()
    {
        // New frame
        netMan.updateFrame();
        procMan.clearInputs();

        // Check for changes to important variables for state transitions
        ChangeMonitor::get().check();

        // Perform the frame step
        if ( rerunStopFrame.value )
            frameStepRerun();
        else
            frameStepNormal();

        // Disable FPS limit while re-running
        if ( rerunStopFrame.value )
            *CC_SKIP_FRAMES_ADDR = 1;

        // Write netplay inputs
        procMan.writeGameInput ( localPlayer, netMan.getInput ( localPlayer ) );
        procMan.writeGameInput ( remotePlayer, netMan.getInput ( remotePlayer ) );
    }

    void netplayStateChanged ( NetplayState state )
    {
        ASSERT ( netMan.getState() != state );

        if ( netMan.getState() != NetplayState::InGame && state == NetplayState::InGame )
        {
            if ( netMan.isRollbackState() )
                procMan.allocateStates();
        }
        else if ( netMan.getState() == NetplayState::InGame && state != NetplayState::InGame )
        {
            if ( netMan.config.rollback )
                procMan.deallocateStates();
        }

        if ( state == NetplayState::CharaSelect
                || ( netMan.getState() == NetplayState::Loading && state == NetplayState::Skippable ) )
        {
            if ( !clientMode.isOffline() )
                shouldSyncRngState = true;
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
            if ( !netMan.config.mode.isTraining() )
                netplayStateChanged ( NetplayState::Skippable );
            else
                netplayStateChanged ( NetplayState::InGame );
            return;
        }

        if ( current == CC_GAME_MODE_RETRY )
        {
            // TODO do this earlier?
            netMan.saveLastGame();
            netplayStateChanged ( NetplayState::RetryMenu );
            return;
        }

        LOG_AND_THROW_STRING ( "Unknown game mode! previous=%u; current=%u", previous, current );
    }

    void delayedStop()
    {
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
                // When the SkippableFlag is set while InGame, we are in a Skippable state
                if ( ! ( previous == 0 && current == 1 && netMan.getState() == NetplayState::InGame ) )
                    break;
                LOG ( "[%s] %s: previous=%u; current=%u", netMan.getIndexedFrame(), var, previous, current );
                netplayStateChanged ( NetplayState::Skippable );
                break;

            default:
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

            newSocket->send ( new VersionConfig ( clientMode ) );

            pushPendingSocket ( newSocket );
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

            main->procMan.ipcSend ( new ErrorMessage ( "Disconnected!" ) );
            delayedStop();
            return;
        }

        popPendingSocket ( socket );

        LOG ( "specSockets.erase ( %08x )", socket );

        specSockets.erase ( socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "readEvent ( %08x, %s, %s )", socket, msg, address );

        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::VersionConfig:
            {
                const Version RemoteVersion = msg->getAs<VersionConfig>().version;

                if ( !LocalVersion.similar ( RemoteVersion, 1 + options[Options::StrictVersion] ) )
                {
                    string local = toString ( "%s.%s", LocalVersion.major(), LocalVersion.minor() );
                    string remote = toString ( "%s.%s", RemoteVersion.major(), RemoteVersion.minor() );

                    if ( options[Options::StrictVersion] >= 1 )
                    {
                        local += LocalVersion.suffix();
                        remote += RemoteVersion.suffix();
                    }

                    if ( options[Options::StrictVersion] >= 2 )
                    {
                        local += " " + LocalVersion.commitId;
                        remote += " " + RemoteVersion.commitId;
                    }

                    if ( options[Options::StrictVersion] >= 3 )
                    {
                        local += " " + LocalVersion.buildTime;
                        remote += " " + RemoteVersion.buildTime;
                    }

                    LOG ( "Incompatible versions:\n%s\n%s", LocalVersion, RemoteVersion );

                    socket->disconnect();
                    return;
                }

                // SpectateConfig *config = new SpectateConfig ( netMan.config );
                // MsgPtr msgPerGameData = netMan.getLastGame();

                // if ( msgPerGameData )
                // {
                //     for ( uint8_t i = 0; i < 2; ++i )
                //     {
                //         config->chara[i] = msgPerGameData->getAs<PerGameData>().chara[i];
                //         config->moon[i] = msgPerGameData->getAs<PerGameData>().moon[i];
                //     }
                // }

                // socket->send ( config );
                return;
            }

            case MsgType::ConfirmConfig:
            {
                SocketPtr newSocket = popPendingSocket ( socket );

                if ( newSocket )
                {
                    specSockets[socket] = newSocket;
                    // rate limit newSocket if host or client
                    // send over newSocket:
                    //  - InputContainers
                    //  - RngStates
                }
                return;
            }

            case MsgType::RngState:
                netMan.setRngState ( msg->getAs<RngState>() );

                for ( const auto& kv : specSockets )
                    kv.first->send ( msg );
                return;

            case MsgType::SyncHash:
                if ( !options[Options::CheckSync] )
                {
                    options.set ( Options::CheckSync, 1 );

                    ASSERT ( localSync.empty() == true );
                    ASSERT ( remoteSync.empty() == true );

                    // Fake the first SyncHash when enabling --check for the first time
                    dataSocket->send ( msg );
                    return;
                }

                remoteSync.push_back ( msg );
                return;

            default:
                break;
        }

        switch ( clientMode.value )
        {
            case ClientMode::Host:
            case ClientMode::Client:
                switch ( msg->getMsgType() )
                {
                    case MsgType::PlayerInputs:
                        netMan.setInputs ( remotePlayer, msg->getAs<PlayerInputs>() );
                        return;

                    default:
                        break;
                }
                break;

            case ClientMode::Spectate:
                switch ( msg->getMsgType() )
                {
                    case MsgType::BothInputs:
                        netMan.setBothInputs ( msg->getAs<BothInputs>() );
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
                syncLog.sessionId = options.arg ( Options::SessionId );
                syncLog.initialize ( options.arg ( Options::AppDir ) + SYNC_LOG_FILE, LOG_VERSION );
                break;

            case MsgType::ControllerMappings:
                mappings = msg->getAs<ControllerMappings>();
                ControllerManager::get().setMappings ( mappings );
                ControllerManager::get().check();
                // TODO proper controller selection
                if ( ControllerManager::get().getJoysticks().empty() )
                {
                    controllers[0] = ControllerManager::get().getKeyboard();
                }
                else
                {
                    controllers[0] = ControllerManager::get().getJoysticks() [0];
                    controllers[1] = ControllerManager::get().getKeyboard();
                }
                break;

            case MsgType::ClientMode:
                if ( clientMode != ClientMode::Unknown )
                    break;

                clientMode = msg->getAs<ClientMode>();
                clientMode.flags |= ClientMode::GameStarted;
                LOG ( "%s: flags={ %s }", clientMode, clientMode.flagString() );
                break;

            case MsgType::IpAddrPort:
                if ( !address.empty() )
                    break;

                address = msg->getAs<IpAddrPort>();
                LOG ( "address='%s'", address );
                break;

            case MsgType::SpectateConfig:
                ASSERT_UNIMPLEMENTED;
                break;

            case MsgType::NetplayConfig:
                if ( netMan.config.delay != 0xFF )
                    break;

                netMan.config = msg->getAs<NetplayConfig>();
                netMan.config.mode = clientMode;

                if ( netMan.config.delay == 0xFF )
                    LOG_AND_THROW_STRING ( "NetplayConfig: delay=%d is invalid!", netMan.config.delay );

                if ( clientMode.isNetplay() )
                {
                    if ( netMan.config.hostPlayer != 1 && netMan.config.hostPlayer != 2 )
                        LOG_AND_THROW_STRING ( "NetplayConfig: hostPlayer=%d is invalid!", netMan.config.hostPlayer );

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

                        // TODO send serverCtrlSocket->address.port to the host

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
                    netplayStateChanged ( NetplayState::Initial );
                }

                LOG ( "NetplayConfig: flags={ %s }; delay=%d; rollback=%d; training=%d; offline=%d; "
                      "hostPlayer=%d; localPlayer=%d; remotePlayer=%d",
                      netMan.config.mode.flagString(), netMan.config.delay, netMan.config.rollback,
                      netMan.config.mode.isTraining(), netMan.config.mode.isOffline(),
                      netMan.config.hostPlayer, localPlayer, remotePlayer );
                break;

            default:
                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // ControllerManager callbacks
    void attachedJoystick ( Controller *controller ) override
    {
    }

    void detachedJoystick ( Controller *controller ) override
    {
    }

    // Controller callback
    void doneMapping ( Controller *controller, uint32_t key ) override
    {
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
            main->procMan.ipcSend ( new ErrorMessage ( "Disconnected!" ) );
            delayedStop();
            initialTimer.reset();
        }
        else if ( timer == stopTimer.get() )
        {
            appState = AppState::Stopping;
        }
        else
        {
            expirePendingSocketTimer ( timer );
        }
    }

    // DLL callback
    void callback()
    {
        // Don't poll unless we're in the correct state
        if ( appState != AppState::Polling )
            return;

        // Check if the world timer changed, this calls hasChanged if changed
        worldTimerMoniter.check();
    }

    // Constructor
    DllMain() : worldTimerMoniter ( this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR )
    {
        // Timer and controller initialization is not done here because of threading issues

        procMan.connectPipe();

        netplayStateChanged ( NetplayState::PreInitial );

        ChangeMonitor::get().addRef ( this, Variable ( Variable::GameMode ), *CC_GAME_MODE_ADDR );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::RoundStart ), roundStartCounter );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::SkippableFlag ), *CC_SKIPPABLE_FLAG_ADDR );
    }

    // Destructor
    ~DllMain()
    {
        syncLog.deinitialize();

        procMan.disconnectPipe();

        // Timer and controller deinitialization is not done here because of threading issues
    }

private:

    static uint16_t convertInputState ( uint32_t state )
    {
        const uint16_t dirs = ( state & MASK_DIRS );
        const uint16_t buttons = ( state & MASK_BUTTONS ) >> 8;

        uint8_t direction = 5;

        if ( dirs & BIT_UP )
            direction = 8;
        else if ( dirs & BIT_DOWN )
            direction = 2;

        if ( dirs & BIT_LEFT )
            --direction;
        else if ( dirs & BIT_RIGHT )
            ++direction;

        return COMBINE_INPUT ( direction, buttons );
    }

};


static void initializeDllMain()
{
    main.reset ( new DllMain() );
}

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
            // Logger::get().initialize ( LOG_FILE );
            // LOG ( "DLL_PROCESS_ATTACH" );

            try
            {
                // It is safe to initialize sockets here
                SocketManager::get().initialize();
                initializePreLoadHacks();
                initializeDllMain();
            }
            catch ( const Exception& err )
            {
                // LOG ( "Aborting due to exception: %s", err );
                exit ( 0 );
            }
#ifdef NDEBUG
            catch ( const std::exception& err )
            {
                // LOG ( "Aborting due to std::exception: %s", err.what() );
                exit ( 0 );
            }
            catch ( ... )
            {
                // LOG ( "Aborting due to unknown exception!" );
                exit ( 0 );
            }
#endif
            break;

        case DLL_PROCESS_DETACH:
            LOG ( "DLL_PROCESS_DETACH" );
            deinitialize();
            break;
    }

    return TRUE;
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

    deinitializeHacks();

    appState = AppState::Deinitialized;
}

static void stopDllMain ( const string& error )
{
    if ( main )
    {
        main->procMan.ipcSend ( new ErrorMessage ( error ) );
        main->delayedStop();
    }
    else
    {
        appState = AppState::Stopping;
    }
}


extern "C" void callback()
{
    if ( appState == AppState::Deinitialized )
        return;

    try
    {
        if ( appState == AppState::Uninitialized )
        {
            initializePostLoadHacks();

            // Joystick and timer must be initialized in the main thread
            TimerManager::get().initialize();
            ControllerManager::get().initialize ( main.get() );

            // Start polling now
            EventManager::get().startPolling();
            appState = AppState::Polling;
        }

        ASSERT ( main.get() != 0 );

        main->callback();
    }
    catch ( const Exception& err )
    {
        LOG ( "Stopping due to exception: %s", err );
        stopDllMain ( "Error: " + err.str() );
    }
#ifdef NDEBUG
    catch ( const std::exception& err )
    {
        LOG ( "Stopping due to std::exception: %s", err.what() );
        stopDllMain ( string ( "Error: " ) + err.what() );
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
