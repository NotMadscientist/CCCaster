#include "Logger.h"
#include "Utilities.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
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


struct Main : public Socket::Owner, public Timer::Owner, public ControllerManager::Owner
{
    HANDLE pipe;
    SocketPtr ipcSocket, ctrlSocket, dataSocket;
    Timer timer;

    void acceptEvent ( Socket *serverSocket ) override
    {
        if ( serverSocket == ipcSocket.get() )
            ipcSocket = serverSocket->accept ( this );
    }

    void connectEvent ( Socket *socket ) override
    {
        LOG ( "Socket %08x connected", socket );
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "Socket %08x disconnected", socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "Got %s from '%s'; socket=%08x", msg, address, socket );

        switch ( msg->getMsgType() )
        {
            case MsgType::ExitGame:
                EventManager::get().stop();
                break;

            default:
                break;
        }

        // if ( msg->getMsgType() == MsgType::SocketShareData )
        // {
        //     if ( msg->getAs<SocketShareData>().isTCP() )
        //         sharedSocket = TcpSocket::shared ( this, msg->getAs<SocketShareData>() );
        //     else
        //         sharedSocket = UdpSocket::shared ( this, msg->getAs<SocketShareData>() );

        //     MsgPtr msg ( new IpAddrPort ( sharedSocket->getRemoteAddress() ) );
        //     ipcSocket->send ( msg, address );
        // }

        // assert ( false );
    }

    void timerExpired ( Timer *timer ) override
    {
        assert ( timer == &this->timer );
    }

    Main() : pipe ( 0 ), ipcSocket ( TcpSocket::listen ( this, 0 ) ), timer ( this )
    {
        LOG ( "Connecting pipe" );

        pipe = CreateFile (
                   NAMED_PIPE,                              // name of the pipe
                   GENERIC_READ | GENERIC_WRITE,            // 2-way pipe
                   FILE_SHARE_READ | FILE_SHARE_WRITE,      // R/W sharing mode
                   0,                                       // default security
                   OPEN_EXISTING,                           // open existing pipe
                   FILE_ATTRIBUTE_NORMAL,                   // default attributes
                   0 );                                     // no template file

        if ( pipe == INVALID_HANDLE_VALUE )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "CreateFile failed" );
        }

        LOG ( "Pipe connected" );

        DWORD bytes;

        if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "WriteFile failed" );
        }

        if ( bytes != sizeof ( ipcSocket->address.port ) )
        {
            Exception err = toString ( "WriteFile wrote %d bytes, expected %d",
                                       bytes, sizeof ( ipcSocket->address.port ) );
            LOG_AND_THROW ( err, "" );
        }

        int processId = GetCurrentProcessId();

        if ( !WriteFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "WriteFile failed" );
        }

        if ( bytes != sizeof ( processId ) )
        {
            Exception err = toString ( "WriteFile wrote %d bytes, expected %d",
                                       bytes, sizeof ( ipcSocket->address.port ) );
            LOG_AND_THROW ( err, "" );
        }
    }

    ~Main()
    {
        if ( pipe )
            CloseHandle ( pipe );

        ipcSocket.reset();
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

                // TODO this is a temporary work around for Wine FPS limit issue
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
