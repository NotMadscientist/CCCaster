#include "Logger.h"
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

enum optionIndex { UNKNOWN, HELP, GTEST, STDOUT, PLUS };

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

    Main ( Option opt[] ) : pipe ( 0 ), timer ( this )
    {
        if ( opt[STDOUT] )
            Logger::get().initialize();
        else
            Logger::get().initialize ( LOG_FILE );
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        // if ( opt[STDOUT] )
        //     return;

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
    }

    ~Main()
    {
        if ( pipe )
            CloseHandle ( pipe );

        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
        Logger::get().deinitialize();
    }
};

static void signalHandler ( int signum )
{
    LOG ( "Interupt signal %d received", signum );
    EventManager::get().release();
}

static BOOL WINAPI consoleCtrl ( DWORD ctrl )
{
    LOG ( "Console ctrl %d received", ctrl );
    EventManager::get().release();
    return TRUE;
}

int main ( int argc, char *argv[] )
{
    static const Descriptor options[] =
    {
        { UNKNOWN, 0,  "",        "", Arg::None, "Usage: " BINARY " [options]\n\nOptions:" },
        { HELP,    0, "h",    "help", Arg::None, "  --help, -h    Print usage and exit." },
        { GTEST,   0,  "",   "gtest", Arg::None, "  --gtest       Run unit tests and exit." },
        { STDOUT,  0,  "",  "stdout", Arg::None, "  --stdout      Output logs to stdout." },
        { PLUS,    0, "p",    "plus", Arg::None, "  --plus, -p    Increment count." },
        {
            UNKNOWN, 0, "",  "", Arg::None,
            "\nExamples:\n"
            "  " BINARY " --unknown -- --this_is_no_option\n"
            "  " BINARY " -unk --plus -ppp file1 file2\n"
        },
        { 0, 0, 0, 0, 0, 0 }
    };

    // Skip program name argv[0] if present, because optionparser doesn't like it
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

    signal ( SIGABRT, signalHandler );
    signal ( SIGINT, signalHandler );
    signal ( SIGTERM, signalHandler );
    SetConsoleCtrlHandler ( consoleCtrl, TRUE );

    if ( opt[GTEST] )
    {
        Logger::get().initialize();
        int result = RunAllTests ( argc, argv );
        Logger::get().deinitialize();
        return result;
    }

    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        cout << "Unknown option: " << it->name << endl;

    for ( int i = 0; i < parser.nonOptionsCount(); ++i )
        cout << "Non-option #" << i << ": " << parser.nonOption ( i ) << endl;

    Main main ( opt );
    EventManager::get().start();
    return 0;
}
