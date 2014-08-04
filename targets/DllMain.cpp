#include "Main.h"
#include "Logger.h"
#include "Utilities.h"
#include "Thread.h"
#include "Constants.h"
#include "NetplayManager.h"

#include <windows.h>

#include <vector>
#include <memory>
#include <cassert>

using namespace std;


#define LOG_FILE FOLDER "dll.log"


// Declarations
void initializePreHacks();
void initializePostHacks();
void deinitializeHacks();
static void deinitialize();

// Main application state
static enum State { UNINITIALIZED, POLLING, STOPPING, DEINITIALIZED } state = UNINITIALIZED;

// Main application instance
struct Main;
static shared_ptr<Main> main;

// Mutex for deinitialize()
static Mutex deinitMutex;

// Position of the current menu's cursor
uint32_t currentMenuIndex = 0;

// Pointer to the value of the character select mode (moon, colour, etc...)
uint32_t *charaSelectModePtr = 0;


struct Main : public CommonMain
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

    // ProcessManager

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
                {
                    LOG ( "Unexpected '%s'", msg );
                    break;
                }

                address = msg->getAs<IpAddrPort>();
                LOG ( "Using: '%s'", address );
                break;

            case MsgType::ClientType:
                if ( clientType != ClientType::Unknown )
                {
                    LOG ( "Unexpected '%s'", msg );
                    break;
                }

                clientType = msg->getAs<ClientType>().value;
                LOG ( "ClientType is %s", isHost() ? "Host" : "Client" );

                if ( isHost() )
                    serverDataSocket = UdpSocket::listen ( this, address.port );
                else
                    dataSocket = UdpSocket::connect ( this, address );

                // TODO randomize player side per session
                localPlayer = ( isHost() ? 1 : 2 );
                remotePlayer = ( isHost() ? 2 : 1 );
                break;

            case MsgType::SocketShareData:
                if ( clientType == ClientType::Unknown )
                {
                    LOG ( "Unexpected '%s'", msg );
                    break;
                }

                if ( !ctrlSocket )
                    ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                else if ( !serverCtrlSocket && isHost() )
                    serverCtrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                else
                    LOG ( "Unexpected '%s'", msg );
                break;

            default:
                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // ControllerManager

    void attachedJoystick ( Controller *controller ) override
    {
    }

    void detachedJoystick ( Controller *controller ) override
    {
    }

    // Controller

    void doneMapping ( Controller *controller, uint32_t key ) override
    {
    }

    // Socket

    void acceptEvent ( Socket *serverSocket ) override
    {
        if ( serverSocket == serverDataSocket.get() )
        {
            dataSocket = serverDataSocket->accept ( this );
            return;
        }

        assert ( serverSocket == serverCtrlSocket.get() );

        serverSocket->accept ( this ).reset();
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
            // case MsgType::PlayerInputs:
            //     netMan.setInputs ( remotePlayer, msg->getAs<PlayerInputs>() );
            //     break;

            default:
                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // Timer

    void timerExpired ( Timer *timer ) override
    {
    }

    // DLL callback

    void callback()
    {
        if ( state != POLLING )
            return;

        // Don't poll for events until a frame step happens
        if ( previousWorldTimer == *CC_WORLD_TIMER_ADDR )
            return;

        if ( initialWorldTimer == 0 )
            initialWorldTimer = *CC_WORLD_TIMER_ADDR;

        previousWorldTimer = *CC_WORLD_TIMER_ADDR;

        frame = ( *CC_WORLD_TIMER_ADDR ) - initialWorldTimer;

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

            input = ( direction | buttons );
        }

        netMan.setInput ( localPlayer, frame, index, input );

        // if ( dataSocket && dataSocket->isConnected() )
        //     dataSocket->send ( netMan.getInputs ( localPlayer, frame, index ) );

        // TODO loop to wait for more inputs if necessary

        if ( !EventManager::get().poll ( pollTimeout ) )
        {
            state = STOPPING;
            return;
        }

        procMan.writeGameInput ( localPlayer, netMan.getDelayedInput ( localPlayer, frame, index ) );
        procMan.writeGameInput ( remotePlayer, netMan.getDelayedInput ( remotePlayer, frame, index ) );
    }

    // Constructor

    Main()
    {
        // Initialization is not done here because of threading issues

        procMan.connectPipe();

        netMan.delay = 2;
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
    if ( state == DEINITIALIZED )
        return;

    try
    {
        if ( state == UNINITIALIZED )
        {
            initializePostHacks();

            // Joystick and timer must be initialized in the main thread
            TimerManager::get().initialize();
            ControllerManager::get().initialize ( main.get() );

            EventManager::get().startPolling();
            state = POLLING;
        }

        assert ( main.get() != 0 );

        main->callback();
    }
    catch ( const WindowsException& err )
    {
        LOG ( "Stopping due to WindowsException: %s", err );
        state = STOPPING;
    }
    catch ( const Exception& err )
    {
        LOG ( "Stopping due to Exception: %s", err );
        state = STOPPING;
    }
    catch ( ... )
    {
        LOG ( "Stopping due to unknown exception!" );
        state = STOPPING;
    }

    if ( state == STOPPING )
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

    if ( state == DEINITIALIZED )
        return;

    main.reset();

    EventManager::get().release();
    TimerManager::get().deinitialize();
    SocketManager::get().deinitialize();
    // Joystick must be deinitialized on the same thread it was initialized
    Logger::get().deinitialize();

    deinitializeHacks();

    state = DEINITIALIZED;
}
