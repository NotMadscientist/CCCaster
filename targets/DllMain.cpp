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
    LOG_TO ( syncLog, "[%u:%s:%u:%s:%u] " FORMAT,                                                               \
             *CC_GAME_MODE_ADDR, gameModeStr ( *CC_GAME_MODE_ADDR ),                                            \
             netMan.getIndex(), netMan.getState(), netMan.getFrame(), __VA_ARGS__ )


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

    // Host and client player numbers
    uint8_t hostPlayer = 1, clientPlayer = 2;

    // Timer for resending inputs while waiting
    TimerPtr resendTimer;

    // The RNG state for the next netplay state
    MsgPtr nextRngState;

    // Indicates if we should wait for the next RNG state before continuing
    bool waitForRngState = false;

    // Indicates if we should set the game's RNG state with nextRngState
    bool shouldSetRngState = false;


    void frameStep()
    {
        // New frame
        netMan.updateFrame();
        procMan.clearInputs();

        // Only save states when in-game
        if ( *CC_GAME_MODE_ADDR == CC_GAME_MODE_INGAME )
            procMan.saveState ( netMan );

        // Check for changes to important variables for state transitions
        ChangeMonitor::get().check();

        if ( netMan.getState().value >= NetplayState::PreInitial && netMan.getState().value <= NetplayState::Initial )
        {
            // Disable FPS limit while going to character select
            *CC_SKIP_FRAMES_ADDR = 1;
        }
        else if ( netMan.getState().value >= NetplayState::CharaSelect )
        {

            // Check for changes to controller state
            ControllerManager::get().check();

            // Input testing code
            uint16_t input;
            {
#if 1
                uint16_t direction = ( rand() % 10 );
                if ( direction == 5 )
                    direction = 0;

                uint16_t buttons = ( rand() % 0x1000 );
                if ( netMan.getState().value == NetplayState::CharaSelect )
                    buttons &= ~ ( CC_BUTTON_B | CC_BUTTON_CANCEL );
#else
                uint16_t direction = 5;

                if ( GetKeyState ( 'P' ) & 0x80 )
                    direction = 8;
                else if ( GetKeyState ( VK_OEM_1 ) & 0x80 )
                    direction = 2;

                if ( GetKeyState ( 'L' ) & 0x80 )
                    --direction;
                else if ( GetKeyState ( VK_OEM_7 ) & 0x80 )
                    ++direction;

                if ( direction == 5 )
                    direction = 0;

                uint16_t buttons = 0;

                if ( GetKeyState ( 'E' ) & 0x80 )       buttons = ( CC_BUTTON_A | CC_BUTTON_SELECT );
                if ( GetKeyState ( 'R' ) & 0x80 )       buttons = ( CC_BUTTON_B | CC_BUTTON_CANCEL );
                if ( GetKeyState ( 'T' ) & 0x80 )       buttons = CC_BUTTON_C;
                if ( GetKeyState ( VK_SPACE ) & 0x80 )  buttons = CC_BUTTON_D;
                if ( GetKeyState ( 'A' ) & 0x80 )       buttons = CC_BUTTON_E;
                if ( GetKeyState ( 'D' ) & 0x80 )       buttons = CC_BUTTON_FN2;
                if ( GetKeyState ( 'G' ) & 0x80 )       buttons = CC_BUTTON_FN1;
                if ( GetKeyState ( VK_F5 ) & 0x80 )     buttons = CC_BUTTON_START;

                // if ( GetKeyState ( VK_F6 ) & 0x80 )
                //     procMan.resetState ( netMan.getFrame() - 30, netMan.getIndex(), netMan );
#endif
                input = COMBINE_INPUT ( direction, buttons );
            }

            netMan.setInput ( localPlayer, input );

            if ( isLocal() )
                netMan.setInput ( remotePlayer, 0 );

            if ( isBroadcast() )
                ; // TODO
            else if ( !isOffline() )
                dataSocket->send ( netMan.getInputs ( localPlayer ) );
        }

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

        // Update the RNG state if necessary
        if ( shouldSetRngState )
        {
            ASSERT ( nextRngState.get() != 0 );
            procMan.setRngState ( nextRngState->getAs<RngState>() );
            netMan.setRngState ( nextRngState->getAs<RngState>() );
            nextRngState.reset();
            shouldSetRngState = false;

            // Log the RNG state after we set it
            LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );
        }

        // Log the RNG state once every 5 seconds
        if ( netMan.getFrame() % ( 5 * 60 ) == 0 )
            LOG_SYNC ( "RngState: %s", procMan.getRngState()->getAs<RngState>().dump() );

        // Write netplay inputs
        procMan.writeGameInput ( localPlayer, netMan.getInput ( localPlayer ) );
        procMan.writeGameInput ( remotePlayer, netMan.getInput ( remotePlayer ) );

        // Log inputs every frame
        LOG_SYNC ( "Inputs: %04x %04x", netMan.getInput ( 1 ), netMan.getInput ( 2 ) );
        overlayText = toString ( "%u:%u", netMan.getIndex(), netMan.getFrame() );
    }

    void bothCharaSelectLoaded()
    {
        LOG ( "Character select loaded for both sides" );

        // Now we can re-enable keepAlive
        ctrlSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
        dataSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
    }

    void netplayStateChanged ( const NetplayState& state )
    {
        if ( state.value == NetplayState::CharaSelect || state.value == NetplayState::InGame )
        {
            if ( isHost() )
            {
                MsgPtr msg = procMan.getRngState();
                ctrlSocket->send ( msg );
                netMan.setRngState ( msg->getAs<RngState>() );
            }
            else if ( isClient() )
            {
                waitForRngState = ( nextRngState.get() == 0 );
                shouldSetRngState = true;
            }
        }

        netMan.setState ( state );

        // Log the RNG state when the host sends it
        if ( ( state.value == NetplayState::CharaSelect || state.value == NetplayState::InGame ) && isHost() )
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
            if ( !netMan.setup.training )
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

        LOG_AND_THROW_STRING ( "Unknown game mode! previous=%u; current=%u", previous, current );
    }

    // ChangeMonitor callbacks
    void hasChanged ( const Variable& var, uint32_t previous, uint32_t current ) override
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

    // ProcessManager callbacks
    void ipcConnectEvent() override
    {
        procMan.allocateRollback();
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
            case MsgType::ClientType:
                if ( clientType != ClientType::Unknown )
                    break;

                clientType = msg->getAs<ClientType>();
                LOG ( "clientType=%s", clientType );
                break;

            case MsgType::NetplaySetup:
                if ( netMan.setup.delay != 0xFF )
                    break;

                netMan.setup = msg->getAs<NetplaySetup>();

                if ( netMan.setup.delay == 0xFF )
                    LOG_AND_THROW_STRING ( "netMan.setup.delay=%d is invalid!", netMan.setup.delay );

                if ( !isLocal() )
                {
                    if ( netMan.setup.hostPlayer != 1 && netMan.setup.hostPlayer != 2 )
                        LOG_AND_THROW_STRING ( "netMan.setup.hostPlayer=%d is invalid!", netMan.setup.hostPlayer );

                    if ( isHost() )
                    {
                        hostPlayer = localPlayer = netMan.setup.hostPlayer;
                        clientPlayer = remotePlayer = ( 3 - netMan.setup.hostPlayer );
                    }
                    else
                    {
                        hostPlayer = remotePlayer = netMan.setup.hostPlayer;
                        clientPlayer = localPlayer = ( 3 - netMan.setup.hostPlayer );
                    }
                }

                LOG ( "delay=%d; training=%d; hostPlayer=%d; clientPlayer=%d; localPlayer=%d; remotePlayer=%d",
                      netMan.setup.delay, netMan.setup.training, hostPlayer, clientPlayer, localPlayer, remotePlayer );
                break;

            case MsgType::SocketShareData:
                switch ( clientType.value )
                {
                    case ClientType::Host:
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

                    case ClientType::Client:
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
                if ( clientType == ClientType::Unknown )
                    LOG_AND_THROW_STRING ( "Unknown clientType!" );

                if ( netMan.setup.delay == 0xFF )
                    LOG_AND_THROW_STRING ( "Uninitalized netMan.setup!" );

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

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override
    {
    }

    void connectEvent ( Socket *socket ) override
    {
    }

    void disconnectEvent ( Socket *socket ) override
    {
        EventManager::get().stop();
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        if ( !msg.get() )
            return;

        switch ( clientType.value )
        {
            case ClientType::Host:
            case ClientType::Client:
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

            case ClientType::Broadcast:
                break;

            default:
                break;
        }

        LOG ( "Unexpected '%s'", msg );
    }

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
        if ( timer == resendTimer.get() )
        {
            dataSocket->send ( netMan.getInputs ( localPlayer ) );
            resendTimer->start ( RESEND_INPUTS_INTERVAL );
        }
    }

    // DLL callback
    void callback()
    {
        // Don't poll until we're in the correct state
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

    if ( appState == AppState::Stopping )
    {
        LOG ( "Exiting" );

        // Joystick must be deinitialized on the same thread
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
    // Joystick must be deinitialized on the same thread it was initialized
    Logger::get().deinitialize();

    deinitializeHacks();

    appState = AppState::Deinitialized;
}
