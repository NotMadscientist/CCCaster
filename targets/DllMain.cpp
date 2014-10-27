#include "Main.h"
#include "Logger.h"
#include "Utilities.h"
#include "Thread.h"
#include "AsmHacks.h"
#include "NetplayManager.h"
#include "ChangeMonitor.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "SmartSocket.h"

#include <windows.h>

#include <vector>
#include <memory>

using namespace std;


#define LOG_FILE FOLDER "dll.log"

#define SYNC_LOG_FILE FOLDER "sync.log"

#define INITIAL_TIMEOUT ( 10000 )

#define DELAYED_STOP ( 500 )

#define RESEND_INPUTS_INTERVAL ( 100 )

#define LOG_SYNC(FORMAT, ...)                                                                                   \
    LOG_TO ( syncLog, "[%u] %s [%s] %s " FORMAT,                                                                \
             *CC_GAME_MODE_ADDR, gameModeStr ( *CC_GAME_MODE_ADDR ),                                            \
             netMan.getIndexedFrame(), netMan.getState(), __VA_ARGS__ )


// Declarations
void initializePreLoadHacks();
void initializePostLoadHacks();
void deinitializeHacks();
static void deinitialize();
extern string overlayText;

// Main application state
static ENUM ( AppState, Uninitialized, Polling, Stopping, Deinitialized ) appState = AppState::Uninitialized;

// Main application instance
struct Main;
static shared_ptr<Main> main;

// Mutex for deinitialize()
static Mutex deinitMutex;

// Enum of variables to monitor
ENUM ( Variable, WorldTime, GameMode, RoundStart );


struct Main
        : public CommonMain
        , public RefChangeMonitor<Variable, uint32_t>::Owner
{
    // NetplayManager instance
    NetplayManager netMan;

    // Logs RNG state and inputs
    Logger syncLog;

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

    // Indicates if we should set the game's RNG state from netMan
    bool shouldSetRngState = false;

    // Frame to stop on, when re-running the game due to rollback.
    // Also used as a flag to indicate re-run mode, 0:0 means not re-running.
    IndexedFrame rerunStopFrame = { { 0, 0 } };

    // Spectator sockets
    unordered_map<Socket *, SocketPtr> specSockets;

    // Timer to stop events
    TimerPtr stopTimer;


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

                // Input testing code
                static bool randomize = false;
                static uint16_t input = 0;

                if ( randomize )
                {
                    if ( netMan.getFrame() % 2 )
                    {
                        uint16_t direction = ( rand() % 10 );

                        // Reduce the chances of moving the cursor at retry menu
                        if ( netMan.getState().value == NetplayState::RetryMenu && ( netMan.getFrame() % 10 ) )
                            direction = 0;

                        uint16_t buttons = ( rand() % 0x1000 );

                        // Prevent hitting some non-essential buttons
                        buttons &= ~ ( CC_BUTTON_D | CC_BUTTON_FN1 | CC_BUTTON_FN2 | CC_BUTTON_START );

                        // Prevent going back at character select
                        if ( netMan.getState().value == NetplayState::CharaSelect )
                            buttons &= ~ ( CC_BUTTON_B | CC_BUTTON_CANCEL );

                        input = COMBINE_INPUT ( direction, buttons );
                    }
                }
                else
                {
                    uint16_t direction = 5, buttons = 0;

                    if ( GetKeyState ( 'P' ) & 0x80 )
                        direction = 8;
                    else if ( GetKeyState ( VK_OEM_1 ) & 0x80 )
                        direction = 2;

                    if ( GetKeyState ( 'L' ) & 0x80 )
                        --direction;
                    else if ( GetKeyState ( VK_OEM_7 ) & 0x80 )
                        ++direction;

                    if ( GetKeyState ( 'E' ) & 0x80 )       buttons = ( CC_BUTTON_A | CC_BUTTON_SELECT );
                    if ( GetKeyState ( 'R' ) & 0x80 )       buttons = ( CC_BUTTON_B | CC_BUTTON_CANCEL );
                    if ( GetKeyState ( 'T' ) & 0x80 )       buttons = CC_BUTTON_C;
                    if ( GetKeyState ( VK_SPACE ) & 0x80 )  buttons = CC_BUTTON_D;
                    if ( GetKeyState ( 'A' ) & 0x80 )       buttons = CC_BUTTON_E;
                    if ( GetKeyState ( 'D' ) & 0x80 )       buttons = CC_BUTTON_FN2;
                    if ( GetKeyState ( 'G' ) & 0x80 )       buttons = CC_BUTTON_FN1;
                    if ( GetKeyState ( VK_F5 ) & 0x80 )     buttons = CC_BUTTON_START;

                    if ( ( GetKeyState ( VK_F6 ) & 0x80 )
                            && netMan.config.rollback
                            && netMan.getState() == NetplayState::InGame
                            && netMan.getFrame() > 60 )
                    {
                        // Sleep ( 1000 );
                        procMan.loadState ( { netMan.getIndex(), netMan.getFrame() - 10 }, netMan );
                    }

                    input = COMBINE_INPUT ( direction, buttons );
                }

                if ( GetKeyState ( VK_F7 ) & 0x80 )         randomize = true;
                if ( GetKeyState ( VK_F8 ) & 0x80 )         randomize = false;

                // Commit inputs to netMan and send them to remote
                netMan.setInput ( localPlayer, input );

                // TODO implement P2 inputs
                if ( clientMode.isLocal() )
                    netMan.setInput ( remotePlayer, input );

                if ( clientMode.isNetplay() )
                    dataSocket->send ( netMan.getInputs ( localPlayer ) );
                break;
            }

            default:
                ASSERT ( !"Unknown NetplayState!" );
                break;
        }

        // Clear the last changed frame before we get new inputs
        netMan.clearLastChangedFrame();

        for ( ;; )
        {
            // Poll until we are ready to run
            if ( !EventManager::get().poll ( pollTimeout ) )
            {
                appState = AppState::Stopping;
                return;
            }

            // Don't need to wait for anything in local modes
            if ( clientMode.isLocal() )
                break;

            // Check if we are ready to continue running, ie not waiting on inputs or RNG state
            const bool ready = ( netMan.areInputsReady() && netMan.isRngStateReady ( shouldSetRngState ) );

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

        // Only do rollback related stuff while in-game
        if ( netMan.getState() == NetplayState::InGame && netMan.isRollbackState()
                && netMan.getLastChangedFrame().value < netMan.getIndexedFrame().value )
        {
            LOG_SYNC ( "Rollback: %s -> %s", netMan.getIndexedFrame(), netMan.getLastChangedFrame() );

            // Indicate we're re-running (save the frame first)
            rerunStopFrame = netMan.getIndexedFrame();

            // Reset the game state (this resets game state and netMan state)
            procMan.loadState ( netMan.getLastChangedFrame(), netMan );
            return;
        }

        // Update the RNG state if necessary
        if ( shouldSetRngState )
        {
            shouldSetRngState = false;

            MsgPtr msgRngState = netMan.getRngState();

            ASSERT ( msgRngState.get() != 0 );

            procMan.setRngState ( msgRngState->getAs<RngState>() );

            // Log the RNG state after we set it
            LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );
        }

        // Broadcast inputs to spectators once every NUM_INPUTS frames
        if ( !specSockets.empty() && netMan.getFrame() % NUM_INPUTS == NUM_INPUTS - 1 )
        {
            MsgPtr msgBothInputs = netMan.getBothInputs();

            ASSERT ( msgBothInputs.get() != 0 );

            for ( const auto& kv : specSockets )
                kv.first->send ( msgBothInputs );
        }

        // Log the RNG state once every 5 seconds
        if ( netMan.getFrame() % ( 5 * 60 ) == 0 )
            LOG_SYNC ( "RngState: %s", procMan.getRngState ( netMan.getIndex() )->getAs<RngState>().dump() );

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
        // Log the RNG state whenever NetplayState changes
        LOG_SYNC ( "RngState: %s", procMan.getRngState ( netMan.getIndex() + 1 )->getAs<RngState>().dump() );

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

        if ( state == NetplayState::CharaSelect || state == NetplayState::InGame )
        {
            MsgPtr msgRngState;

            if ( clientMode.isHost() || clientMode.isBroadcast() )
            {
                msgRngState = procMan.getRngState ( netMan.getIndex() + 1 );

                // Log the RNG state when the host sends it
                LOG_SYNC ( "RngState: %s", msgRngState->getAs<RngState>().dump() );
            }

            switch ( clientMode.value )
            {
                case ClientMode::Host:
                    ASSERT ( msgRngState.get() != 0 );

                    dataSocket->send ( msgRngState ); // Intentional fall through to Broadcast

                case ClientMode::Broadcast:
                    ASSERT ( msgRngState.get() != 0 );

                    for ( const auto& kv : specSockets )
                        kv.first->send ( msgRngState );
                    break;

                case ClientMode::Client:
                case ClientMode::Spectate:
                    shouldSetRngState = true;
                    break;

                default:
                    break;
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
                LOG ( "%s: previous=%u; current=%u",  var, previous, current );
                gameModeChanged ( previous, current );
                break;

            case Variable::RoundStart:
                LOG ( "%s: previous=%u; current=%u", var, previous, current );
                // In-game happens after round start, when players can start moving
                netplayStateChanged ( NetplayState::InGame );
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
            SocketPtr newSocket = serverCtrlSocket->accept ( this );

            LOG ( "newSocket=%08x", newSocket.get() );

            ASSERT ( newSocket != 0 );
            ASSERT ( newSocket->isConnected() == true );

            newSocket->send ( new VersionConfig ( clientMode, ClientMode::GameStarted ) );

            pendingSockets[newSocket.get()] = newSocket;
        }
        else if ( serverSocket == serverDataSocket.get() )
        {
            dataSocket = serverDataSocket->accept ( this );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            ASSERT ( dataSocket != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            netplayStateChanged ( NetplayState::Initial );

            stopTimer.reset();
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

        stopTimer.reset();
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "disconnectEvent ( %08x )", socket );

        if ( socket == dataSocket.get() )
        {
            main->procMan.ipcSend ( new ErrorMessage ( "Disconnected!" ) );
            delayedStop();
            return;
        }

        pendingSockets.erase ( socket );
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
                if ( !LocalVersion.similar ( msg->getAs<VersionConfig>().version ) )
                    socket->disconnect();
                else
                    socket->send ( new SpectateConfig ( netMan.config ) );
                return;

            case MsgType::ConfirmConfig:
                if ( pendingSockets.find ( socket ) == pendingSockets.end() )
                    break;

                specSockets[socket] = pendingSockets[socket];
                pendingSockets.erase ( socket );
                return;

            case MsgType::RngState:
                netMan.setRngState ( msg->getAs<RngState>() );

                for ( const auto& kv : specSockets )
                    kv.first->send ( msg );
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
        EventManager::get().stop();
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::ClientMode:
                if ( clientMode != ClientMode::Unknown )
                    break;

                clientMode = msg->getAs<ClientMode>();
                LOG ( "clientMode=%s", clientMode );
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
                        serverDataSocket = UdpSocket::listen ( this, address.port );
                        LOG ( "serverDataSocket=%08x", serverDataSocket.get() );
                    }
                    else if ( clientMode.isClient() )
                    {
                        dataSocket = UdpSocket::connect ( this, address );
                        LOG ( "dataSocket=%08x", dataSocket.get() );
                    }

                    stopTimer.reset ( new Timer ( this ) );
                    stopTimer->start ( INITIAL_TIMEOUT );

                    // Wait for dataSocket to be connected before changing to NetplayState::Initial
                }
                else if ( clientMode.isBroadcast() )
                {
                    ASSERT ( netMan.config.mode.isBroadcast() == true );

                    LOG ( "NetplayConfig: broadcastPort=%u", netMan.config.broadcastPort );

                    serverCtrlSocket = TcpSocket::listen ( this, netMan.config.broadcastPort );

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
        else if ( timer == stopTimer.get() )
        {
            // TODO log and display WHY?
            appState = AppState::Stopping;
        }
        else
        {
            ASSERT_IMPOSSIBLE;
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
    Main() : worldTimerMoniter ( this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR )
    {
        // Timer and controller initialization is not done here because of threading issues

        procMan.connectPipe();

        netplayStateChanged ( NetplayState::PreInitial );

        ChangeMonitor::get().addRef ( this, Variable ( Variable::GameMode ), *CC_GAME_MODE_ADDR );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::RoundStart ), roundStartCounter );

        syncLog.initialize ( SYNC_LOG_FILE, 0 );
    }

    // Destructor
    ~Main()
    {
        syncLog.deinitialize();

        procMan.disconnectPipe();

        // Timer and controller deinitialization is not done here because of threading issues
    }
};

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

            // Only poll timers and sockets; controllers state is only allowed to change once a frame
            EventManager::get().checkBitMask = ( CHECK_TIMERS | CHECK_SOCKETS );
            EventManager::get().startPolling();
            appState = AppState::Polling;
        }

        ASSERT ( main.get() != 0 );

        main->callback();
    }
    catch ( const Exception& err )
    {
        LOG ( "Stopping due to exception: %s", err );

        if ( main )
        {
            main->procMan.ipcSend ( new ErrorMessage ( "Error: " + err.str() ) );
            main->delayedStop();
        }
        else
        {
            appState = AppState::Stopping;
        }
    }
#ifdef NDEBUG
    catch ( const std::exception& err )
    {
        LOG ( "Stopping due to std::exception: %s", err.what() );

        if ( main )
        {
            main->procMan.ipcSend ( new ErrorMessage ( string ( "Error: " ) + err.what() ) );
            main->delayedStop();
        }
        else
        {
            appState = AppState::Stopping;
        }
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );

        if ( main )
        {
            main->procMan.ipcSend ( new ErrorMessage ( "Unknown error!" ) );
            main->delayedStop();
        }
        else
        {
            appState = AppState::Stopping;
        }
    }
#endif

    if ( appState == AppState::Stopping )
    {
        LOG ( "Exiting" );

        // Joystick must be deinitialized on the same thread it was initialized
        ControllerManager::get().deinitialize();
        deinitialize();
        exit ( 0 );
    }
}

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
            Logger::get().initialize ( LOG_FILE );
            LOG ( "DLL_PROCESS_ATTACH" );

            try
            {
                // It is safe to initialize sockets here
                SocketManager::get().initialize();
                initializePreLoadHacks();

                main.reset ( new Main() );
            }
            catch ( const Exception& err )
            {
                LOG ( "Aborting due to exception: %s", err );
                exit ( 0 );
            }
#ifdef NDEBUG
            catch ( const std::exception& err )
            {
                LOG ( "Aborting due to std::exception: %s", err.what() );
                exit ( 0 );
            }
            catch ( ... )
            {
                LOG ( "Aborting due to unknown exception!" );
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
