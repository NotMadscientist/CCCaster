#include "Logger.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "ProcessManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Timer.h"
#include "Test.h"
#include "Messages.h"
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

// Main connection address and port
static IpAddrPort mainAddrPort;


struct Main
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Timer::Owner
{
    ClientType::Enum clientType;
    ProcessManager procMan;
    SocketPtr serverSocket, ctrlSocket, dataSocket;
    TimerPtr timer;

    // ProcessManager

    void ipcConnectEvent() override
    {
        procMan.ipcSend ( new ClientType ( clientType ) );

        assert ( ctrlSocket.get() != 0 );
        assert ( dataSocket.get() != 0 );

        procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
        procMan.ipcSend ( dataSocket->share ( procMan.getProcessId() ) );

        if ( clientType == ClientType::Host )
        {
            assert ( serverSocket.get() != 0 );
            procMan.ipcSend ( serverSocket->share ( procMan.getProcessId() ) );
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
        assert ( serverSocket == this->serverSocket.get() );

        ctrlSocket = serverSocket->accept ( this );

        procMan.openGame();
    }

    void connectEvent ( Socket *socket ) override
    {
        assert ( socket == this->ctrlSocket.get() );

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

    // State query methods

    bool isHost() const { return ( clientType == ClientType::Host ); }

    // Constructor

    Main ( Option opt[] ) : clientType ( ClientType::Unknown ), procMan ( this )
    {
        if ( opt[STDOUT] )
            Logger::get().initialize();
        else
            Logger::get().initialize ( LOG_FILE );
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        if ( mainAddrPort.addr.empty() )
        {
            clientType = ClientType::Host;
            serverSocket = TcpSocket::listen ( this, mainAddrPort.port );
            dataSocket = UdpSocket::bind ( this, mainAddrPort.port );
        }
        else
        {
            clientType = ClientType::Client;
            ctrlSocket = TcpSocket::connect ( this, mainAddrPort );
            dataSocket = UdpSocket::bind ( this, mainAddrPort );
        }
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
        if ( parser.nonOptionsCount() == 1 )
            mainAddrPort = string ( parser.nonOption ( 0 ) );
        else if ( parser.nonOptionsCount() == 2 )
            mainAddrPort = string ( parser.nonOption ( 0 ) ) + parser.nonOption ( 1 );

        PRINT ( "Using: '%s'", mainAddrPort );

        Main main ( opt );
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
