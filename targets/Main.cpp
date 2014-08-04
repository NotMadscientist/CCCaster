#include "Main.h"
#include "Logger.h"
#include "Test.h"
#include "Constants.h"

#include <optionparser.h>
#include <windows.h>

#include <cassert>
#include <exception>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"


// Set of command line options
enum CommandLineOptions { UNKNOWN, HELP, GTEST, STDOUT, PLUS };


struct Main : public CommonMain
{
    // ProcessManager

    void ipcConnectEvent() override
    {
        procMan.ipcSend ( REF_PTR ( address ) );
        procMan.ipcSend ( new ClientType ( clientType ) );

        assert ( ctrlSocket.get() != 0 );
        procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );

        if ( isHost() )
        {
            assert ( serverCtrlSocket.get() != 0 );
            procMan.ipcSend ( serverCtrlSocket->share ( procMan.getProcessId() ) );
        }
    }

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

    void acceptEvent ( Socket *serverSocket ) override
    {
        assert ( serverSocket == serverCtrlSocket.get() );

        ctrlSocket = serverCtrlSocket->accept ( this );

        procMan.openGame();
    }

    void connectEvent ( Socket *socket ) override
    {
        assert ( socket == ctrlSocket.get() );

        procMan.openGame();
    }

    void disconnectEvent ( Socket *socket ) override
    {
        EventManager::get().stop();
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
    }

    // Timer

    void timerExpired ( Timer *timer ) override
    {
    }

    // Constructor

    Main ( Option opt[], const IpAddrPort& address ) : CommonMain ( address )
    {
        if ( opt[STDOUT] )
            Logger::get().initialize();
        else
            Logger::get().initialize ( LOG_FILE );
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        if ( isHost() )
            serverCtrlSocket = TcpSocket::listen ( this, address.port );
        else
            ctrlSocket = TcpSocket::connect ( this, address );
    }

    // Destructor

    ~Main()
    {
        procMan.closeGame();

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
            UNKNOWN, 0, "", "", Arg::None,
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
        PRINT ( "Unknown option: '%s'", it->name );

    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        PRINT ( "Non-option (%d): '%s'", i, parser.nonOption ( i ) );

    try
    {
        IpAddrPort address;
        if ( parser.nonOptionsCount() == 1 )
            address = string ( parser.nonOption ( 0 ) );
        else if ( parser.nonOptionsCount() == 2 )
            address = string ( parser.nonOption ( 0 ) ) + parser.nonOption ( 1 );

        PRINT ( "Using: '%s'", address );

        Main main ( opt, address );
        EventManager::get().start();
    }
    catch ( const WindowsException& err )
    {
        PRINT ( "Error: %s", err );
    }
    catch ( const Exception& err )
    {
        PRINT ( "Error: %s", err );
    }

    return 0;
}

// Empty definitions needed to keep the linker happy
extern "C" void callback() {}
uint32_t currentMenuIndex;
uint32_t *charaSelectModePtr;
