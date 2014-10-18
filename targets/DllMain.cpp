#include "Main.h"
#include "Logger.h"
#include "Utilities.h"
#include "Thread.h"
#include "AsmHacks.h"
#include "NetplayManager.h"
#include "ChangeMonitor.h"

#include <windows.h>

#include <vector>
#include <memory>

using namespace std;


#define LOG_FILE FOLDER "dll.log"

#define SYNC_LOG_FILE FOLDER "sync.log"

#define RESEND_INPUTS_INTERVAL 100

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

    // The RNG state for the next netplay state
    MsgPtr nextRngState;

    // Indicates if we should wait for the next RNG state before continuing
    bool waitForRngState = false;

    // Indicates if we should set the game's RNG state with nextRngState
    bool shouldSetRngState = false;

    // Frame to stop on, when re-running the game due to rollback.
    // Also used as a flag to indicate re-run mode, 0:0 means not re-running.
    IndexedFrame rerunStopFrame = { { 0, 0 } };


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

                if ( isLocal() )
                    netMan.setInput ( remotePlayer, input );

                if ( isBroadcast() )
                    ; // TODO broadcast to spectators
                else if ( !isOffline() )
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

            // Don't need wait for inputs in local modes
            if ( isLocal() )
                break;

            // Check if we should wait for anything
            if ( netMan.areInputsReady() && !waitForRngState )
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
            ASSERT ( nextRngState.get() != 0 );
            procMan.setRngState ( nextRngState->getAs<RngState>() );
            netMan.saveRngState ( nextRngState->getAs<RngState>() );
            nextRngState.reset();
            shouldSetRngState = false;

            // Log the RNG state after we set it
            LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );
        }

        // Log the RNG state once every 5 seconds
        if ( netMan.getFrame() % ( 5 * 60 ) == 0 )
            LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );

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

    void bothCharaSelectLoaded()
    {
        LOG ( "Character select loaded for both sides" );

        // Now we can re-enable keepAlive
        ctrlSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
        dataSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
    }

    void netplayStateChanged ( NetplayState state )
    {
        // Log the RNG state whenever NetplayState changes
        LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );

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
            MsgPtr msg;
            if ( isHost() || isBroadcast() )
                msg = procMan.getRngState();

            switch ( clientMode.value )
            {
                case ClientMode::Host:
                    ctrlSocket->send ( msg ); // Intentional fall through to saveRngState

                case ClientMode::Broadcast:
                    netMan.saveRngState ( msg->getAs<RngState>() );
                    break;

                case ClientMode::Client:
                    waitForRngState = ( nextRngState.get() == 0 );
                    shouldSetRngState = true;
                    break;

                default:
                    break;
            }
        }

        netMan.setState ( state );

        // Log the RNG state when the host sends it
        if ( ( state == NetplayState::CharaSelect || state == NetplayState::InGame ) && isHost() )
            LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );
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
            // Once both sides have loaded up to character select for the first time
            if ( !isLocal() && netMan.getState() == NetplayState::Initial )
            {
                if ( remoteCharaSelectLoaded )
                    bothCharaSelectLoaded();
                ctrlSocket->send ( new CharaSelectLoaded() );
            }

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
            if ( !netMan.config.isTraining() )
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

        // TODO proper queueing of potential spectators
    }

    void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "disconnectEvent ( %08x )", socket );
        EventManager::get().stop();
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "readEvent ( %08x, %s, %s )", socket, msg, address );

        if ( !msg.get() )
            return;

        switch ( clientMode.value )
        {
            case ClientMode::Host:
            case ClientMode::Client:
                switch ( msg->getMsgType() )
                {
                    case MsgType::CharaSelectLoaded:
                        // Once both sides have loaded up to character select for the first time
                        if ( !remoteCharaSelectLoaded )
                        {
                            if ( netMan.getState().value >= NetplayState::CharaSelect )
                                bothCharaSelectLoaded();
                            remoteCharaSelectLoaded = true;
                        }
                        return;

                    case MsgType::PlayerInputs:
                        netMan.setInputs ( remotePlayer, msg->getAs<PlayerInputs>() );
                        return;

                    case MsgType::RngState:
                        nextRngState = msg;
                        waitForRngState = false;
                        return;

                    default:
                        break;
                }
                break;

            case ClientMode::Broadcast:
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

            case MsgType::NetplayConfig:
                if ( netMan.config.delay != 0xFF )
                    break;

                netMan.config = msg->getAs<NetplayConfig>();

                if ( netMan.config.delay == 0xFF )
                    LOG_AND_THROW_STRING ( "netMan.config.delay=%d is invalid!", netMan.config.delay );

                if ( !isLocal() )
                {
                    if ( netMan.config.hostPlayer != 1 && netMan.config.hostPlayer != 2 )
                        LOG_AND_THROW_STRING ( "netMan.config.hostPlayer=%d is invalid!", netMan.config.hostPlayer );

                    // Determine the player numbers
                    if ( isHost() )
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
                }
                else if ( isBroadcast() )
                {
                    LOG ( "broadcastPort=%u", netMan.config.broadcastPort );

                    serverCtrlSocket = TcpSocket::listen ( this, netMan.config.broadcastPort );

                    if ( netMan.config.broadcastPort != serverCtrlSocket->address.port )
                    {
                        netMan.config.broadcastPort = serverCtrlSocket->address.port;
                        netMan.config.invalidate();
                        procMan.ipcSend ( REF_PTR ( netMan.config ) );
                    }
                }

                LOG ( "delay=%d; rollback=%d; training=%d; offline=%d; hostPlayer=%d; localPlayer=%d; remotePlayer=%d",
                      netMan.config.delay, netMan.config.rollback, netMan.config.isTraining(),
                      netMan.config.isOffline(), netMan.config.hostPlayer, localPlayer, remotePlayer );
                break;

            case MsgType::SocketShareData:
                if ( isLocal() )
                    LOG_AND_THROW_STRING ( "Invalid clientMode=%s!", clientMode );

                switch ( clientMode.value )
                {
                    case ClientMode::Host:
                        if ( !serverCtrlSocket )
                        {
                            serverCtrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );

                            if ( serverCtrlSocket->isUDP() )
                            {
                                ASSERT ( serverCtrlSocket->getAsUDP().getChildSockets().size() == 1 );
                                ctrlSocket = serverCtrlSocket->getAsUDP().getChildSockets().begin()->second;
                                ctrlSocket->owner = this;
                                ASSERT ( ctrlSocket->isConnected() );
                            }
                        }
                        else if ( !serverDataSocket )
                        {
                            serverDataSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );

                            ASSERT ( serverDataSocket->isUDP() == true );
                            ASSERT ( serverDataSocket->getAsUDP().getChildSockets().size() == 1 );
                            dataSocket = serverDataSocket->getAsUDP().getChildSockets().begin()->second;
                            dataSocket->owner = this;
                            ASSERT ( dataSocket->isConnected() );
                        }
                        else if ( !ctrlSocket )
                        {
                            ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                        }
                        break;

                    case ClientMode::Client:
                        if ( !ctrlSocket )
                        {
                            ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                            ASSERT ( ctrlSocket->isConnected() == true );
                        }
                        else if ( !dataSocket )
                        {
                            dataSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                            ASSERT ( dataSocket->isConnected() == true );
                        }
                        break;

                    default:
                        break;
                }
                break;

            case MsgType::EndOfMessages:
                if ( clientMode == ClientMode::Unknown )
                    LOG_AND_THROW_STRING ( "Unknown clientMode!" );

                if ( netMan.config.delay == 0xFF )
                    LOG_AND_THROW_STRING ( "Uninitalized netMan.config!" );

                if ( !isLocal() )
                {
                    if ( !ctrlSocket || !ctrlSocket->isConnected() )
                        LOG_AND_THROW_STRING ( "Uninitalized ctrlSocket!" );

                    if ( !dataSocket || !dataSocket->isConnected() )
                        LOG_AND_THROW_STRING ( "Uninitalized dataSocket!" );

                    if ( isHost() )
                    {
                        if ( !serverCtrlSocket )
                            LOG_AND_THROW_STRING ( "Uninitalized serverCtrlSocket!" );

                        if ( !serverDataSocket )
                            LOG_AND_THROW_STRING ( "Uninitalized serverDataSocket!" );
                    }
                }

                netplayStateChanged ( NetplayState::Initial );
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
        ASSERT ( timer == resendTimer.get() );

        dataSocket->send ( netMan.getInputs ( localPlayer ) );
        resendTimer->start ( RESEND_INPUTS_INTERVAL );
    }

    // DLL callback
    void callback()
    {
        // Don't poll unless we're in the correct state
        if ( appState != AppState::Polling )
            return;

        // First check if the world timer changed, if not, poll for events once and return
        if ( !worldTimerMoniter.check() )
            if ( !EventManager::get().poll ( pollTimeout ) )
                appState = AppState::Stopping;
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
        appState = AppState::Stopping;
        if ( main )
            main->procMan.ipcSend ( new ErrorMessage ( "Error: " + err.str() ) );
    }
#ifdef NDEBUG
    catch ( const std::exception& err )
    {
        LOG ( "Stopping due to std::exception: %s", err.what() );
        appState = AppState::Stopping;
        if ( main )
            main->procMan.ipcSend ( new ErrorMessage ( string ( "Error: " ) + err.what() ) );
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );
        appState = AppState::Stopping;
        if ( main )
            main->procMan.ipcSend ( new ErrorMessage ( "Unknown error!" ) );
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
