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
    MsgPtr netplaySetup;

    // ProcessManager callbacks
    void ipcConnectEvent() override
    {
        assert ( address.empty() == false );
        assert ( clientType != ClientType::Unknown );
        assert ( netplaySetup.get() != 0 );

        procMan.ipcSend ( REF_PTR ( address ) );
        procMan.ipcSend ( new ClientType ( clientType ) );
        procMan.ipcSend ( netplaySetup );

        if ( isHost() )
        {
            assert ( ctrlSocket.get() != 0 );
            assert ( ctrlSocket->isConnected() == true );
            assert ( dataSocket.get() != 0 );
            assert ( dataSocket->isConnected() == true );
            assert ( serverCtrlSocket.get() != 0 );
            assert ( serverDataSocket.get() != 0 );
            assert ( serverDataSocket->getAsUDP().getChildSockets().size() == 1 );
            assert ( serverDataSocket->getAsUDP().getChildSockets().begin()->second == dataSocket );

            // We don't send the dataSocket since it will be included in serverDataSocket's SocketShareData
            procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
            procMan.ipcSend ( serverCtrlSocket->share ( procMan.getProcessId() ) );
            procMan.ipcSend ( serverDataSocket->share ( procMan.getProcessId() ) );
        }
        else
        {
            assert ( ctrlSocket.get() != 0 );
            assert ( ctrlSocket->isConnected() == true );
            assert ( dataSocket.get() != 0 );
            assert ( dataSocket->isConnected() == true );

            procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
            procMan.ipcSend ( dataSocket->share ( procMan.getProcessId() ) );
        }
    }

    void ipcDisconnectEvent() override
    {
        EventManager::get().stop();
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
    {
    }

    // ControllerManager callbacks
    void attachedJoystick ( Controller *controller ) override
    {
    }

    void detachedJoystick ( Controller *controller ) override
    {
    }

    // Controller callback
    void doneMapping ( Controller *controller, uint32_t key ) override
    {
    }

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override
    {
        if ( serverSocket == serverCtrlSocket.get() )
        {
            ctrlSocket = serverCtrlSocket->accept ( this );
        }
        else if ( serverSocket == serverDataSocket.get() )
        {
            dataSocket = serverDataSocket->accept ( this );
        }
        else
        {
            LOG ( "Ignoring acceptEvent from serverSocket=%08x", serverSocket );
            serverSocket->accept ( this ).reset();
            return;
        }

        if ( ctrlSocket && dataSocket )
        {
            assert ( ctrlSocket->isConnected() == true );
            assert ( dataSocket->isConnected() == true );

            netplaySetup.reset ( new NetplaySetup ( 4, 1 + ( rand() % 2 ) ) );
            ctrlSocket->send ( netplaySetup );
            procMan.openGame();
        }
    }

    void connectEvent ( Socket *socket ) override
    {
        assert ( ctrlSocket.get() != 0 );
        assert ( dataSocket.get() != 0 );
        assert ( socket == ctrlSocket.get() || socket == dataSocket.get() );
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
            case MsgType::NetplaySetup:
                // TODO check state
                netplaySetup = msg;
                procMan.openGame();
                break;

            default:
                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // Timer callback
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
        {
            serverCtrlSocket = TcpSocket::listen ( this, address.port );
            serverDataSocket = UdpSocket::listen ( this, address.port );
        }
        else
        {
            ctrlSocket = TcpSocket::connect ( this, address );
            dataSocket = UdpSocket::connect ( this, address );
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
#if 0
    Logger::get().initialize();

    size_t consumed;
    char bytes[] = {};

    MsgPtr msg = Protocol::decode ( bytes, sizeof ( bytes ), consumed );

    assert ( consumed == sizeof ( bytes ) );

    assert ( false );

    Logger::get().deinitialize();
    return 0;
#endif

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
