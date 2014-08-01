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

#include <windows.h>

#include <vector>
#include <memory>
#include <cassert>

using namespace std;


#define LOG_FILE FOLDER "dll.log"

#define FRAME_INTERVAL ( 1000 / 60 )


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

// Number of milliseconds to poll during each frame
static uint64_t frameInterval = FRAME_INTERVAL;


struct Main
        : public ProcessManager::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Socket::Owner
{
    ProcessManager procMan;
    Controller *controllers[2];
    SocketPtr ctrlSocket, dataSocket;

    // ProcessManager

    void ipcDisconnectEvent() override
    {
        EventManager::get().stop();
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
    {
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

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
    }

    Main() : procMan ( this )
    {
        // Initialization is not done here because of threading issues

        procMan.connectPipe();
    }

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

                // TODO this is a temporary work around for the Wine FPS limit issue
                if ( detectWine() )
                    frameInterval = 5;
            }

            if ( state != POLLING )
                break;

            // Poll for events
            if ( !EventManager::get().poll ( frameInterval ) )
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
