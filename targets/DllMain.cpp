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

#define SEND_INPUTS_INTERVAL 100


// Declarations
void initializePreHacks();
void initializePostHacks();
void deinitializeHacks();
static void deinitialize();

// Main application state
static ENUM ( State, Uninitialized, Polling, Stopping, Deinitialized ) state = State::Uninitialized;

// Main application instance
struct Main;
static shared_ptr<Main> main;

// Mutex for deinitialize()
static Mutex deinitMutex;

// Enum of values to monitor
ENUM ( VarName, currentMenuIndex, currentGameMode, charaSelectModeP1, charaSelectModeP2 );


struct Main
        : public CommonMain
        , public RefChangeMonitor<VarName::Enum, uint32_t>::Owner
        , public PtrToRefChangeMonitor<VarName::Enum, uint32_t>::Owner
{
    // The NetplayManager instance
    NetplayManager netMan;

    // The initial value of CC_WORLD_TIMER_ADDR
    uint32_t initialWorldTimer = 0;

    // The previous value of CC_WORLD_TIMER_ADDR
    uint32_t previousWorldTimer = 0;

    // The timeout for each call to EventManager::poll
    uint64_t pollTimeout = 1;

    // The local and remote player numbers
    uint8_t localPlayer = 1, remotePlayer = 2;

    // The current netplay frame
    uint32_t frame = 0;

    // The current transition index
    uint16_t index = 0;

    // If we are currently sending and receiving inputs
    bool sendRecvInputs = false;


    // DLL callback
    void callback()
    {
        if ( state != State::Polling )
            return;

        // Don't poll for events until a frame step happens
        if ( previousWorldTimer == *CC_WORLD_TIMER_ADDR )
            return;

        // Start watching for changes when we initialize this timer
        if ( initialWorldTimer == 0 )
        {
            initialWorldTimer = *CC_WORLD_TIMER_ADDR;

            ChangeMonitor::get().addRef ( this, VarName::currentMenuIndex, currentMenuIndex );
            ChangeMonitor::get().addRef ( this, VarName::currentGameMode, *CC_GAME_MODE_ADDR );
            ChangeMonitor::get().addPtrToRef ( this, VarName::charaSelectModeP1,
                                               const_cast<const uint32_t *&> ( charaSelectModes[0] ), ( uint32_t ) 0 );
            ChangeMonitor::get().addPtrToRef ( this, VarName::charaSelectModeP2,
                                               const_cast<const uint32_t *&> ( charaSelectModes[1] ), ( uint32_t ) 0 );
        }

        previousWorldTimer = *CC_WORLD_TIMER_ADDR;

        frame = ( *CC_WORLD_TIMER_ADDR ) - initialWorldTimer;

        ChangeMonitor::get().check();

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

        netMan.setInput ( localPlayer, frame, index, input );

        if ( sendRecvInputs )
            dataSocket->send ( netMan.getInputs ( localPlayer, frame, index ) );

        for ( ;; )
        {
            if ( !EventManager::get().poll ( pollTimeout ) )
            {
                state = State::Stopping;
                return;
            }

            if ( !sendRecvInputs )
                break;

            if ( netMan.getEndFrame ( remotePlayer ) + netMan.delay > frame )
            {
                timer.reset();
                break;
            }

            if ( !timer )
            {
                timer.reset ( new Timer ( this ) );
                timer->start ( SEND_INPUTS_INTERVAL );
            }
        }

        procMan.writeGameInput ( localPlayer, netMan.getDelayedInput ( localPlayer, frame, index ) );
        procMan.writeGameInput ( remotePlayer, netMan.getDelayedInput ( remotePlayer, frame, index ) );
    }

    // ChangeMonitor callbacks
    void hasChanged ( const VarName::Enum& var, uint32_t previous, uint32_t current ) override
    {
        LOG ( "%s changed; previous=%u; current=%u", VarName ( var ), previous, current );
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
                // TODO check state
                address = msg->getAs<IpAddrPort>();
                LOG ( "Using: '%s'", address );
                break;

            case MsgType::ClientType:
                // TODO check state
                clientType = msg->getAs<ClientType>().value;
                LOG ( "ClientType is %s", isHost() ? "Host" : "Client" );
                break;

            case MsgType::NetplaySetup:
                // TODO check state
                netMan.delay = msg->getAs<NetplaySetup>().delay;
                if ( isHost() )
                {
                    localPlayer = msg->getAs<NetplaySetup>().hostPlayer;
                    remotePlayer = 3 - msg->getAs<NetplaySetup>().hostPlayer;
                }
                else
                {
                    remotePlayer = msg->getAs<NetplaySetup>().hostPlayer;
                    localPlayer = 3 - msg->getAs<NetplaySetup>().hostPlayer;
                }
                LOG ( "delay=%d; localPlayer=%d; remotePlayer=%d", netMan.delay, localPlayer, remotePlayer );
                break;

            case MsgType::SocketShareData:
                // TODO check state
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
                        sendRecvInputs = true;
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
                        sendRecvInputs = true;
                    }
                }
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
        assert ( timer == this->timer.get() );
        assert ( sendRecvInputs == true );

        dataSocket->send ( netMan.getInputs ( localPlayer, frame, index ) );

        timer->start ( SEND_INPUTS_INTERVAL );
    }

    // Constructor
    Main()
    {
        // Initialization is not done here because of threading issues

        procMan.connectPipe();
    }

    // Destructor
    ~Main()
    {
        procMan.disconnectPipe();

        // Deinitialization is not done here because of threading issues
    }
};

extern "C" void callback()
{
    if ( state == State::Deinitialized )
        return;

    try
    {
        if ( state == State::Uninitialized )
        {
            initializePostHacks();

            // Joystick and timer must be initialized in the main thread
            TimerManager::get().initialize();
            ControllerManager::get().initialize ( main.get() );

            EventManager::get().startPolling();
            state = State::Polling;
        }

        assert ( main.get() != 0 );

        main->callback();
    }
    catch ( const WindowsException& err )
    {
        LOG ( "Stopping due to WindowsException: %s", err );
        state = State::Stopping;
    }
    catch ( const Exception& err )
    {
        LOG ( "Stopping due to Exception: %s", err );
        state = State::Stopping;
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );
        state = State::Stopping;
    }

    if ( state == State::Stopping )
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

    if ( state == State::Deinitialized )
        return;

    main.reset();

    EventManager::get().release();
    TimerManager::get().deinitialize();
    SocketManager::get().deinitialize();
    // Joystick must be deinitialized on the same thread it was initialized
    Logger::get().deinitialize();

    deinitializeHacks();

    state = State::Deinitialized;
}
