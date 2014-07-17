#include "Logger.h"
#include "Utilities.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Timer.h"
#include "AsmHacks.h"

#include <windows.h>
#include <MinHook.h>

#include <vector>
#include <memory>
#include <cassert>

using namespace std;
using namespace AsmHacks;


#define LOG_FILE FOLDER "dll.log"

#define FRAME_INTERVAL  ( 1000 / 60 )

#define WRITE_ASM_HACK(ASM_HACK)                                                \
    do {                                                                        \
        WindowsException err;                                                   \
        if ( ( err = ASM_HACK.write() ).code != 0 ) {                           \
            LOG ( "%s; %s failed", err, #ASM_HACK );                            \
            exit ( 0 );                                                         \
        }                                                                       \
    } while ( 0 )

#define HOOK_FUNC(RETURN_TYPE, FUNC_NAME, ...)                                  \
    typedef RETURN_TYPE ( WINAPI *p ## FUNC_NAME ) ( __VA_ARGS__ );             \
    p ## FUNC_NAME o ## FUNC_NAME = 0;                                          \
    RETURN_TYPE WINAPI m ## FUNC_NAME ( __VA_ARGS__ )

static bool hookWindowsCalls();
static void unhookWindowsCalls();


HOOK_FUNC ( BOOL, QueryPerformanceFrequency, LARGE_INTEGER *lpFrequency  )
{
    lpFrequency->QuadPart = 1;
    return TRUE;
}

struct Main : public Socket::Owner, public Timer::Owner, public ControllerManager::Owner
{
    HANDLE pipe;
    SocketPtr ipcSocket;
    Timer timer;

    // void acceptEvent ( Socket *serverSocket ) { serverSocket->accept ( this ).reset(); }

    void connectEvent ( Socket *socket )
    {
        LOG ( "Socket %08x connected", socket );
    }

    void disconnectEvent ( Socket *socket )
    {
        LOG ( "Socket %08x disconnected", socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
    {
        LOG ( "Got %s from '%s'; socket=%08x", msg, address, socket );

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

    Main() : pipe ( 0 ), ipcSocket ( UdpSocket::bind ( this, 0 ) ), timer ( this )
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
            LOG_AND_THROW ( err, "; CreateFile failed" );
        }

        LOG ( "Pipe connected" );

        DWORD bytes;

        if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "; WriteFile failed" );
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
            LOG_AND_THROW ( err, "; WriteFile failed" );
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
    }
};

static enum State { UNINITIALIZED, POLLING, STOPPING, DEINITIALIZED } state = UNINITIALIZED;

static shared_ptr<Main> main;

extern "C" void callback()
{
    try
    {
        do
        {
            if ( state == UNINITIALIZED )
            {
                // Joystick and timer must be initialized in the main thread
                TimerManager::get().initialize();
                ControllerManager::get().initialize ( main.get() );
                EventManager::get().startPolling();

                // Asm hacks that must written after the game starts
                WRITE_ASM_HACK ( disableFpsLimit );

                state = POLLING;
            }

            if ( state != POLLING )
                break;

            // Poll for events
            if ( !EventManager::get().poll ( FRAME_INTERVAL ) )
            {
                state = STOPPING;
                break;
            }
        }
        while ( 0 );
    }
    catch ( const WindowsException& err )
    {
        state = STOPPING;
    }
    catch ( ... )
    {
        LOG ( "Unknown exception!" );
        state = STOPPING;
    }

    if ( state == STOPPING )
    {
        EventManager::get().stop();
        ControllerManager::get().deinitialize();
        TimerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        state = DEINITIALIZED;
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

                main.reset ( new Main() );

                static const Asm hookCallback1 =
                {
                    MM_HOOK_CALL1_ADDR,
                    {
                        0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - MM_HOOK_CALL1_ADDR - 5 ), // call callback
                        0xE9, INLINE_DWORD ( MM_HOOK_CALL2_ADDR - MM_HOOK_CALL1_ADDR - 10 ) // jmp MM_HOOK_CALL2_ADDR
                    }
                };

                WRITE_ASM_HACK ( hookCallback1 );
                WRITE_ASM_HACK ( hookCallback2 );
                WRITE_ASM_HACK ( loopStartJump ); // Write the jump location last, due to dependencies on the above

                for ( void *const addr : disabledStageAddrs )
                {
                    Asm enableStage = { addr, INLINE_DWORD_FF };
                    WindowsException err;

                    if ( ( err = enableStage.write() ).code )
                    {
                        LOG ( "%s; enableStage { %08x } failed", err, addr );
                        exit ( 0 );
                    }
                }

                WRITE_ASM_HACK ( fixRyougiStageMusic1 );
                WRITE_ASM_HACK ( fixRyougiStageMusic2 );

                // uint64_t perfFreq;
                // QueryPerformanceFrequency ( ( LARGE_INTEGER * ) &perfFreq );
                // LOG ( "perfFreq=%llu", perfFreq );

                if ( !hookWindowsCalls() )
                    exit ( 0 );
            }
            catch ( const WindowsException& err )
            {
                exit ( 0 );
            }
            catch ( ... )
            {
                LOG ( "Unknown exception!" );
                exit ( 0 );
            }
            break;

        case DLL_PROCESS_DETACH:
            unhookWindowsCalls();
            main.reset();
            LOG ( "DLL_PROCESS_DETACH" );
            EventManager::get().release();
            Logger::get().deinitialize();
            break;
    }

    return TRUE;
}

static bool hookWindowsCalls()
{
    if ( MH_Initialize() != MH_OK )
    {
        LOG ( "MH_Initialize failed" );
    }
    else
    {
        if ( MH_CreateHook ( ( void * ) &QueryPerformanceFrequency, ( void * ) &mQueryPerformanceFrequency,
                             ( void ** ) &oQueryPerformanceFrequency ) != MH_OK )
            LOG ( "MH_CreateHook for QueryPerformanceFrequency failed" );
        else if ( MH_EnableHook ( ( void * ) &QueryPerformanceFrequency ) != MH_OK )
            LOG ( "MH_EnableHook for QueryPerformanceFrequency failed" );
        else
            return true;
    }

    return false;
}

static void unhookWindowsCalls()
{
    MH_DisableHook ( ( void * ) &QueryPerformanceFrequency );
    MH_RemoveHook ( ( void * ) &QueryPerformanceFrequency );
    MH_Uninitialize();
}
