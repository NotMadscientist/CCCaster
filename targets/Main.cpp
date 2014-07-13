#include "Log.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Timer.h"
#include "Test.h"

#include <optionparser.h>
#include <windows.h>

#include <cassert>

#define LOG_FILE FOLDER "debug.log"

using namespace std;
using namespace option;

enum optionIndex { UNKNOWN, HELP, TEST, PLUS };

struct Main : public Socket::Owner, public Timer::Owner, public ControllerManager::Owner
{
    HANDLE pipe;
    SocketPtr ipcSocket;
    Timer timer;

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
    {
        LOG ( "Got %s from '%s'", msg, address );
    }

    void timerExpired ( Timer *timer )
    {
        assert ( timer == &this->timer );
    }

    Main() : pipe ( 0 ), timer ( this )
    {
        Log::get().initialize ( LOG_FILE );
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        LOG ( "Opening pipe" );

        pipe = CreateNamedPipe (
                   NAMED_PIPE,                                          // name of the pipe
                   PIPE_ACCESS_DUPLEX,                                  // 2-way pipe
                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,     // byte stream + blocking
                   1,                                                   // only allow 1 instance of this pipe
                   1024,                                                // outbound buffer size
                   1024,                                                // inbound buffer size
                   0,                                                   // use default wait time
                   0 );                                                 // use default security attributes

        if ( pipe == INVALID_HANDLE_VALUE )
        {
            WindowsError err = GetLastError();
            LOG ( "CreateNamedPipe failed: %s", err );
            throw err;
        }

        LOG ( "Starting " MBAA_EXE );

        system ( "start \"\" " LAUNCHER " " MBAA_EXE " " HOOK_DLL );
        system ( "./" LAUNCHER " " MBAA_EXE " " HOOK_DLL " &" );

        LOG ( "Connecting pipe" );

        if ( !ConnectNamedPipe ( pipe, 0 ) )
        {
            int error = GetLastError();

            if ( error != ERROR_PIPE_CONNECTED )
            {
                WindowsError err = error;
                LOG ( "ConnectNamedPipe failed: %s", err );
                throw err;
            }
        }

        LOG ( "Pipe connected" );

        DWORD bytes;
        int processId = 0;
        IpAddrPort ipcHost ( "127.0.0.1", 0 );

        if ( !ReadFile ( pipe, &ipcHost.port, sizeof ( ipcHost.port ), &bytes, 0 ) )
        {
            WindowsError err = GetLastError();
            LOG ( "ReadFile failed: %s", err );
            throw err;
        }

        if ( bytes != sizeof ( ipcHost.port ) )
        {
            LOG ( "ReadFile read %d bytes, expected %d", bytes, sizeof ( ipcHost.port ) );
            throw "something"; // TODO
        }

        LOG ( "ipcHost='%s'", ipcHost );

        ipcSocket = UdpSocket::bind ( this, ipcHost );

        if ( !ReadFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
        {
            WindowsError err = GetLastError();
            LOG ( "ReadFile failed: %s", err );
            throw err;
        }

        if ( bytes != sizeof ( processId ) )
        {
            LOG ( "ReadFile read %d bytes, expected %d", bytes, sizeof ( processId ) );
            throw "something"; // TODO
        }

        LOG ( "processId=%08x", processId );

        // tcpSocket = TcpSocket::connect ( this, IpAddrPort ( "google.com", 80 ) );
        // ipcSocket->send ( tcpSocket->share ( processId ) );

        // ipcSocket->send ( ipcSocket->share ( processId ) );
    }

    ~Main()
    {
        if ( pipe )
            CloseHandle ( pipe );

        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
        Log::get().deinitialize();
    }
};

int main ( int argc, char *argv[] )
{
    static const Descriptor options[] =
    {
        { UNKNOWN, 0, "", "", Arg::None, "Usage: " BINARY " [options]\n\nOptions:" },
        { HELP,    0, "h", "help", Arg::None, "  --help, -h    Print usage and exit." },
        { TEST,    0, "t", "test", Arg::None, "  --test, -t    Run unit tests and exit." },
        { PLUS,    0, "p", "plus", Arg::None, "  --plus, -p    Increment count." },
        {
            UNKNOWN, 0, "",  "", Arg::None,
            "\nExamples:\n"
            "  " BINARY " --unknown -- --this_is_no_option\n"
            "  " BINARY " -unk --plus -ppp file1 file2\n"
        },
        { 0, 0, 0, 0, 0, 0 }
    };

    // skip program name argv[0] if present
    argc -= ( argc > 0 );
    argv += ( argc > 0 );

    Stats stats ( options, argc, argv );
    Option opt[stats.options_max], buffer[stats.buffer_max];
    Parser parser ( options, argc, argv, opt, buffer );

    if ( parser.error() )
        return -1;

    if ( opt[HELP] )
    {
        printUsage ( cout, options );
        return 0;
    }

    if ( opt[TEST] )
    {
        Log::get().initialize();
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( 0 );

        int result = RunAllTests ( argc, argv );

        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
        Log::get().deinitialize();
        return result;
    }

    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        cout << "Unknown option: " << it->name << endl;

    for ( int i = 0; i < parser.nonOptionsCount(); ++i )
        cout << "Non-option #" << i << ": " << parser.nonOption ( i ) << endl;

    Main main;
    EventManager::get().start();
    return 0;
}
