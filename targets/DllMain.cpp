#include "Logger.h"
#include "Utilities.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "ProcessManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Timer.h"
#include "Thread.h"
#include "Messages.h"
#include "Constants.h"

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

// Current application state
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

// The timeout for each call to EventManager::poll
uint64_t pollTimeout = 1;


struct Main
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
{
    ClientType::Enum clientType;
    ProcessManager procMan;
    SocketPtr serverSocket, ctrlSocket, dataSocket;

    Controller *controllers[2];

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
            case MsgType::ClientType:
                if ( clientType != ClientType::Unknown )
                {
                    LOG ( "Unexpected '%s'", msg );
                    break;
                }

                clientType = msg->getAs<ClientType>().value;
                LOG ( "ClientType is %s", clientType == ClientType::Host ? "Host" : "Client" );
                break;

            case MsgType::SocketShareData:
                if ( clientType == ClientType::Unknown )
                {
                    LOG ( "Unexpected '%s'", msg );
                    break;
                }

                if ( !ctrlSocket )
                    ctrlSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                else if ( !dataSocket )
                    dataSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
                else if ( !serverSocket && clientType == ClientType::Host )
                    serverSocket = Socket::shared ( this, msg->getAs<SocketShareData>() );
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
    }

    // Constructor

    Main() : clientType ( ClientType::Unknown ), procMan ( this )
    {
        // Initialization is not done here because of threading issues

        controllers[0] = controllers[1] = 0;

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
    if ( state == DEINITIALIZED )
        return;

    try
    {
        do
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

            if ( state != POLLING )
                break;

            // Don't poll for events until a frame step happens
            static uint32_t lastWorldTimer = 0;
            if ( lastWorldTimer == *CC_WORLD_TIMER_ADDR )
                break;
            lastWorldTimer = *CC_WORLD_TIMER_ADDR;

            // Input testing code
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

                assert ( main.get() != 0 );

                main->procMan.writeGameInputs ( 1, direction, buttons );
            }

            if ( !EventManager::get().poll ( pollTimeout ) )
            {
                state = STOPPING;
                break;
            }

        }
        while ( 0 );
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
