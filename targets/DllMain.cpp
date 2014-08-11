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
#include <cassert>

using namespace std;


#define LOG_FILE FOLDER "dll.log"

#define RESEND_INPUTS_INTERVAL 100


// Declarations
void initializePreHacks();
void initializePostHacks();
void deinitializeHacks();
static void deinitialize();

// Main application state
static ENUM ( AppState, Uninitialized, Polling, Stopping, Deinitialized ) appState = AppState::Uninitialized;

// Main application instance
struct Main;
static shared_ptr<Main> main;

// Mutex for deinitialize()
static Mutex deinitMutex;

// Enum of variables to monitor
ENUM ( Variable, WorldTime, MenuIndex, GameMode, RoundStart, CharaSelectModeP1, CharaSelectModeP2 );


struct Main
        : public CommonMain
        , public RefChangeMonitor<Variable, uint32_t>::Owner
        , public PtrToRefChangeMonitor<Variable, uint32_t>::Owner
{
    // NetplayManager instance
    NetplayManager netMan;

    // If remote has loaded up to character select
    bool remoteCharaSelectLoaded = false;

    // ChangeMonitor for CC_WORLD_TIMER_ADDR
    RefChangeMonitor<Variable, uint32_t> worldTimerMoniter;

    // Timeout for each call to EventManager::poll
    uint64_t pollTimeout = 1;

    // Local and remote player numbers
    uint8_t localPlayer = 1, remotePlayer = 2;

    // Host and client player numbers
    uint8_t hostPlayer = 1, clientPlayer = 2;

    // Timer for resending inputs while waiting
    TimerPtr resendTimer;


    void frameStep()
    {
        // New frame
        netMan.updateFrame();
        procMan.clearInputs();

        // Check for changes to important variables for state transitions
        ChangeMonitor::get().check();

        if ( netMan.getState().value >= NetplayState::PreInitial && netMan.getState().value <= NetplayState::Initial )
        {
            // Disable FPS limit while going to character select
            *CC_SKIP_FRAMES_ADDR = 1;
        }
        else if ( netMan.getState().value >= NetplayState::CharaSelect )
        {
            // Input testing code
            uint16_t input;
            {
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

                input = COMBINE_INPUT ( direction, buttons );
            }

            netMan.setInput ( localPlayer, input );
            dataSocket->send ( netMan.getInputs ( localPlayer ) );
        }

        // Poll until we have enough inputs to run
        for ( ;; )
        {
            if ( !EventManager::get().poll ( pollTimeout ) )
            {
                appState = AppState::Stopping;
                return;
            }

            if ( netMan.areInputsReady() )
            {
                resendTimer.reset();
                break;
            }

            if ( !resendTimer )
            {
                resendTimer.reset ( new Timer ( this ) );
                resendTimer->start ( RESEND_INPUTS_INTERVAL );
            }
        }

        // Write netplay inputs
        procMan.writeGameInput ( localPlayer, netMan.getNetplayInput ( localPlayer ) );
        procMan.writeGameInput ( remotePlayer, netMan.getNetplayInput ( remotePlayer ) );
    }

    void bothCharaSelectLoaded()
    {
        LOG ( "Character select loaded for both sides" );

        // Now we can re-enable keepAlive
        ctrlSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
        dataSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
    }

    void gameModeChanged ( uint32_t previous, uint32_t current )
    {
        if ( current == 0
                || current == CC_GAME_MODE_STARTUP
                || current == CC_GAME_MODE_OPENING
                || current == CC_GAME_MODE_TITLE
                || current == CC_GAME_MODE_DEMO
                || current == CC_GAME_MODE_MAIN
                || ( previous == CC_GAME_MODE_DEMO && current == CC_GAME_MODE_INGAME ) )
        {
            return;
        }

        if ( current == CC_GAME_MODE_CHARA_SELECT )
        {
            // Once both sides have loaded up to character select for the first time
            if ( netMan.getState() == NetplayState::Initial )
            {
                if ( remoteCharaSelectLoaded )
                    bothCharaSelectLoaded();
                ctrlSocket->send ( new CharaSelectLoaded() );
            }

            netMan.setState ( NetplayState::CharaSelect );
            return;
        }

        if ( current == CC_GAME_MODE_LOADING )
        {
            netMan.setState ( NetplayState::Loading );
            return;
        }

        if ( current == CC_GAME_MODE_INGAME )
        {
            // In-game starts with character intros, which is a skippable state
            netMan.setState ( NetplayState::Skippable );
            return;
        }

        if ( current == CC_GAME_MODE_RETRY )
        {
            netMan.setState ( NetplayState::RetryMenu );
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
                netMan.setState ( NetplayState::InGame );
                break;

            case Variable::MenuIndex:
                break;

            case Variable::CharaSelectModeP1:
                break;

            case Variable::CharaSelectModeP2:
                break;

            default:
                break;
        }
    }

    // ProcessManager callbacks
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
            case MsgType::IpAddrPort:
                if ( !address.empty() )
                    break;

                address = msg->getAs<IpAddrPort>();
                LOG ( "address='%s'", address );
                break;

            case MsgType::ClientType:
                if ( clientType != ClientType::Unknown )
                    break;

                clientType = msg->getAs<ClientType>();
                LOG ( "clientType=%s", clientType );
                break;

            case MsgType::NetplaySetup:
                if ( netplaySetup.delay != 0 )
                    break;

                netplaySetup = msg->getAs<NetplaySetup>();

                if ( isHost() )
                {
                    hostPlayer = localPlayer = msg->getAs<NetplaySetup>().hostPlayer;
                    clientPlayer = remotePlayer = 3 - msg->getAs<NetplaySetup>().hostPlayer;
                }
                else
                {
                    hostPlayer = remotePlayer = msg->getAs<NetplaySetup>().hostPlayer;
                    clientPlayer = localPlayer = 3 - msg->getAs<NetplaySetup>().hostPlayer;
                }

                LOG ( "delay=%d; training=%d; hostPlayer=%d; clientPlayer=%d; localPlayer=%d; remotePlayer=%d",
                      netplaySetup.delay, netplaySetup.training, hostPlayer, clientPlayer, localPlayer, remotePlayer );
                break;

            case MsgType::SocketShareData:
                if ( isHost() )
                {
                    if ( !ctrlSocket )
                    {
                        ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                    }
                    else if ( !serverCtrlSocket )
                    {
                        serverCtrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                    }
                    else if ( !serverDataSocket )
                    {
                        serverDataSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                        assert ( serverDataSocket->getAsUDP().getChildSockets().size() == 1 );
                        dataSocket = serverDataSocket->getAsUDP().getChildSockets().begin()->second;
                        dataSocket->owner = this;
                        assert ( dataSocket->isConnected() );
                    }
                }
                else
                {
                    if ( !ctrlSocket )
                    {
                        ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                        assert ( ctrlSocket->isConnected() == true );
                    }
                    else if ( !dataSocket )
                    {
                        dataSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                        assert ( dataSocket->isConnected() == true );
                    }
                }
                break;

            case MsgType::EndOfMessages:
                if ( address.empty() )
                    LOG_AND_THROW_STRING ( "Empty address!" );

                if ( clientType == ClientType::Unknown )
                    LOG_AND_THROW_STRING ( "Unknown clientType!" );

                if ( netplaySetup.delay == 0 )
                    LOG_AND_THROW_STRING ( "Uninitalized netplaySetup!" );

                if ( !ctrlSocket )
                    LOG_AND_THROW_STRING ( "Uninitalized ctrlSocket!" );

                if ( !dataSocket )
                    LOG_AND_THROW_STRING ( "Uninitalized dataSocket!" );

                if ( isHost() )
                {
                    if ( !serverCtrlSocket )
                        LOG_AND_THROW_STRING ( "Uninitalized serverCtrlSocket!" );

                    if ( !serverDataSocket )
                        LOG_AND_THROW_STRING ( "Uninitalized serverDataSocket!" );
                }

                netMan.setState ( NetplayState::Initial );
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
                break;

            case MsgType::PlayerInputs:
                netMan.setInputs ( remotePlayer, msg->getAs<PlayerInputs>() );
                break;

            default:
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
    Main() : netMan ( netplaySetup ), worldTimerMoniter ( this, Variable::WorldTime, *CC_WORLD_TIMER_ADDR )
    {
        // Timer and controller initialization is not done here because of threading issues

        procMan.connectPipe();

        netMan.setState ( NetplayState::PreInitial );

        ChangeMonitor::get().addRef ( this, Variable ( Variable::GameMode ), *CC_GAME_MODE_ADDR );
        ChangeMonitor::get().addRef ( this, Variable ( Variable::RoundStart ), roundStartCounter );

        // ChangeMonitor::get().addRef ( this, Variable ( Variable::MenuIndex ), currentMenuIndex );
        // ChangeMonitor::get().addPtrToRef ( this, Variable ( Variable::CharaSelectModeP1 ),
        //                                    const_cast<const uint32_t *&> ( charaSelectModes[0] ), ( uint32_t ) 0 );
        // ChangeMonitor::get().addPtrToRef ( this, Variable ( Variable::CharaSelectModeP2 ),
        //                                    const_cast<const uint32_t *&> ( charaSelectModes[1] ), ( uint32_t ) 0 );
    }

    // Destructor
    ~Main()
    {
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
            initializePostHacks();

            // Joystick and timer must be initialized in the main thread
            TimerManager::get().initialize();
            ControllerManager::get().initialize ( main.get() );

            EventManager::get().startPolling();
            appState = AppState::Polling;
        }

        assert ( main.get() != 0 );

        main->callback();
    }
    catch ( const WindowsException& err )
    {
        LOG ( "Stopping due to WindowsException: %s", err );
        appState = AppState::Stopping;
    }
    catch ( const Exception& err )
    {
        LOG ( "Stopping due to Exception: %s", err );
        appState = AppState::Stopping;
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );
        appState = AppState::Stopping;
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
                initializePreHacks();

                main.reset ( new Main() );
            }
            catch ( const WindowsException& err )
            {
                LOG ( "Aborting due to WindowsException: %s", err );
                exit ( 0 );
            }
            catch ( const Exception& err )
            {
                LOG ( "Aborting due to Exception: %s", err );
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
