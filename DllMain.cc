#include "Log.h"
#include "Util.h"
#include "Event.h"
#include "UdpSocket.h"
#include "Timer.h"

#include <windows.h>

#include <vector>
#include <memory>
#include <cassert>

#define LOG_FILE FOLDER "dll.log"

#define HOOK_CALL1_ADDR ( ( char * ) 0x40D032 )
#define LOOP_START_ADDR ( ( char * ) 0x40D330 )
#define HOOK_CALL2_ADDR ( ( char * ) 0x40D411 )

using namespace std;

struct Main : public Socket::Owner, public Timer::Owner
{
    HANDLE pipe;
    SocketPtr ipcSocket;
    Timer timer;

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
    {
        assert ( socket == ipcSocket.get() );

        LOG ( "Got %s from '%s'", msg, address );
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
            WindowsError err = GetLastError();
            LOG ( "CreateFile failed: %s", err );
            throw err;
        }

        LOG ( "Pipe connected" );

        DWORD bytes;

        if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
        {
            WindowsError err = GetLastError();
            LOG ( "WriteFile failed: %s", err );
            throw err;
        }

        if ( bytes != sizeof ( ipcSocket->address.port ) )
        {
            LOG ( "WriteFile wrote %d bytes, expected %d", bytes, sizeof ( ipcSocket->address.port ) );
            throw "something"; // TODO
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
        // Initialize the EventManager once
        if ( state == UNINITIALIZED )
        {
            EventManager::get().initializeTimers();
            EventManager::get().initializePolling();
            state = POLLING;
        }

        if ( state != POLLING )
            return;

        // Poll for events
        if ( !EventManager::get().poll() )
            state = STOPPING;
    }
    catch ( const WindowsError& err )
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
        EventManager::get().deinitialize();
        state = DEINITIALIZED;
        exit ( 0 );
    }
}

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
            Log::get().initialize ( LOG_FILE );
            LOG ( "DLL_PROCESS_ATTACH" );
            try
            {
                EventManager::get().initializeSockets();

                main.reset ( new Main() );

                Asm hookCallback1 =
                {
                    HOOK_CALL1_ADDR,
                    {
                        0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - HOOK_CALL1_ADDR - 5 ),  // call callback
                        0xE9, INLINE_DWORD ( HOOK_CALL2_ADDR - HOOK_CALL1_ADDR - 10 )           // jmp HOOK_CALL2_ADDR
                    }
                };

                Asm loopStartJump =
                {
                    LOOP_START_ADDR,
                    {
                        0xE9, INLINE_DWORD ( HOOK_CALL1_ADDR - LOOP_START_ADDR - 5 ),           // jmp HOOK_CALL1_ADDR
                        0x90                                                                    // nop
                    }
                };

                Asm hookCallback2 =
                {
                    HOOK_CALL2_ADDR,
                    {
                        0x6A, 0x01,                                                             // push 01
                        0x6A, 0x00,                                                             // push 00
                        0x6A, 0x00,                                                             // push 00
                        0xE9, INLINE_DWORD ( LOOP_START_ADDR - HOOK_CALL2_ADDR - 5 )            // jmp LOOP_START_ADDR+6
                    }
                };

                WindowsError err;

                LOG ( "Writing hooks" );

                if ( ( err = hookCallback1.write() ).code )
                {
                    LOG ( "hookCallback1 failed: %s", err );
                    exit ( 0 );
                }

                if ( ( err = hookCallback2.write() ).code )
                {
                    LOG ( "hookCallback2 failed: %s", err );
                    exit ( 0 );
                }

                // Write the jump location last, because the other blocks of code need to be in place first
                LOG ( "Writing loop start jump" );

                if ( ( err = loopStartJump.write() ).code )
                {
                    LOG ( "loopStartJump failed: %s", err );
                    exit ( 0 );
                }
            }
            catch ( const WindowsError& err )
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
            LOG ( "DLL_PROCESS_DETACH" );
            EventManager::get().release();
            Log::get().deinitialize();
            break;
    }

    return TRUE;
}
