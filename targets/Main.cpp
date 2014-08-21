#include "Main.h"
#include "MainUi.h"
#include "Logger.h"
#include "Test.h"
#include "Constants.h"
#include "Pinger.h"

#include <optionparser.h>
#include <windows.h>

#include <exception>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"

#define PING_INTERVAL ( 100 )

#define NUM_PINGS ( 5 )


// Set of command line options
enum CommandLineOptions { UNKNOWN, HELP, DUMMY, GTEST, STDOUT, NO_FORK, PLUS };

// Main UI instance
static MainUi ui;


/* Connect procedure

    1 - Connect both sockets

    2 - Both send and check version (TODO)

    3 - Host pings then sends stats

    4 - Client pings then sends stats

    5 - Both see the calculated delay and decide to connect (host chooses settings)

*/

struct Main : public CommonMain, public Pinger::Owner
{
    shared_ptr<Pinger> pinger;

    Statistics remoteStats;

    NetplaySetup netplaySetup;


    virtual void gotPingStats ( const Statistics& remoteStats )
    {
        if ( isClient() )
        {
            this->remoteStats = remoteStats;
            pinger->start();
            return;
        }

        this->remoteStats = remoteStats;
        donePinging();
    }

    virtual void donePinging()
    {
        // Disable keepAlive during the initial limbo period
        ctrlSocket->setKeepAlive ( 0 );
        dataSocket->setKeepAlive ( 0 );

        if ( isHost() )
        {
            serverCtrlSocket->setKeepAlive ( 0 );
            serverDataSocket->setKeepAlive ( 0 );
        }

        Statistics stats = pinger->getStats();
        stats.merge ( remoteStats );

        if ( isHost() )
        {
            if ( !ui.accepted ( stats ) )
            {
                EventManager::get().stop();
                return;
            }

            netplaySetup = ui.getNetplaySetup();
            ctrlSocket->send ( REF_PTR ( netplaySetup ) );

            // Start the game and wait for callback to ipcConnectEvent
            startGame();
        }
        else
        {
            if ( !ui.connected ( stats ) )
            {
                EventManager::get().stop();
                return;
            }

            // Wait for NetplaySetup before starting game
        }
    }

    virtual void gotNetplaySetup ( const NetplaySetup& netplaySetup )
    {
        this->netplaySetup = netplaySetup;

        // Start the game and wait for callback to ipcConnectEvent
        startGame();
    }

    virtual void startGame()
    {
        // Remove sockets from the EventManager so messages get buffered.
        // These are safe to call even if null or the socket is not a real fd.
        SocketManager::get().remove ( serverCtrlSocket.get() );
        SocketManager::get().remove ( serverDataSocket.get() );
        SocketManager::get().remove ( ctrlSocket.get() );
        SocketManager::get().remove ( dataSocket.get() );

        // Open the game wait and for callback to ipcConnectEvent
        procMan.openGame();
    }

    // Pinger callbacks
    virtual void sendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        ASSERT ( pinger == this->pinger.get() );

        dataSocket->send ( ping );
    }

    virtual void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) override
    {
        ASSERT ( pinger == this->pinger.get() );

        LOG ( "latency=%.2f ms; jitter=%.2f ms; packetLoss=%d%", stats.getMean(), stats.getJitter(), packetLoss );

        ctrlSocket->send ( new Statistics ( stats ) );

        if ( isClient() )
            donePinging();
    }

    // ProcessManager callbacks
    virtual void ipcConnectEvent() override
    {
        ASSERT ( clientType != ClientType::Unknown );
        ASSERT ( netplaySetup.delay != 0xFF );

        procMan.ipcSend ( REF_PTR ( clientType ) );
        procMan.ipcSend ( REF_PTR ( netplaySetup ) );

        if ( !isLocal() )
        {
            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );
            ASSERT ( dataSocket.get() != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            if ( isHost() )
            {
                ASSERT ( serverCtrlSocket.get() != 0 );
                ASSERT ( serverDataSocket.get() != 0 );
                ASSERT ( serverDataSocket->getAsUDP().getChildSockets().size() == 1 );
                ASSERT ( serverDataSocket->getAsUDP().getChildSockets().begin()->second == dataSocket );

                procMan.ipcSend ( serverCtrlSocket->share ( procMan.getProcessId() ) );
                procMan.ipcSend ( serverDataSocket->share ( procMan.getProcessId() ) );

                // We don't share UDP sockets since they will be included in the server's share data
                if ( ctrlSocket->isTCP() )
                {
                    procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
                }
                else
                {
                    ASSERT ( serverCtrlSocket->getAsUDP().getChildSockets().size() == 1 );
                    ASSERT ( serverCtrlSocket->getAsUDP().getChildSockets().begin()->second == ctrlSocket );
                }
            }
            else
            {
                procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
                procMan.ipcSend ( dataSocket->share ( procMan.getProcessId() ) );
            }
        }

        procMan.ipcSend ( new EndOfMessages() );
    }

    virtual void ipcDisconnectEvent() override
    {
        EventManager::get().stop();
    }

    virtual void ipcReadEvent ( const MsgPtr& msg ) override
    {
        if ( !msg.get() || msg->getMsgType() != MsgType::ErrorMessage )
        {
            LOG ( "Unexpected '%s'", msg );
            return;
        }

        ui.sessionError = msg->getAs<ErrorMessage>().error;
    }

    // ControllerManager callbacks
    virtual void attachedJoystick ( Controller *controller ) override
    {
    }

    virtual void detachedJoystick ( Controller *controller ) override
    {
    }

    // Controller callback
    virtual void doneMapping ( Controller *controller, uint32_t key ) override
    {
    }

    // Socket callbacks
    virtual void acceptEvent ( Socket *serverSocket ) override
    {
        // TODO proper queueing
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
            ASSERT ( ctrlSocket->isConnected() == true );
            ASSERT ( dataSocket->isConnected() == true );

            // TOOD version check

            pinger->start();
        }
    }

    virtual void connectEvent ( Socket *socket ) override
    {
        ASSERT ( ctrlSocket.get() != 0 );
        ASSERT ( dataSocket.get() != 0 );
        ASSERT ( socket == ctrlSocket.get() || socket == dataSocket.get() );
    }

    virtual void disconnectEvent ( Socket *socket ) override
    {
        EventManager::get().stop();
    }

    virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::Ping:
                pinger->gotPong ( msg );
                break;

            case MsgType::Statistics:
                gotPingStats ( msg->getAs<Statistics>() );
                break;

            case MsgType::NetplaySetup:
                gotNetplaySetup ( msg->getAs<NetplaySetup>() );
                break;

            default:
                LOG ( "Unexpected '%s'", msg );
                break;
        }
    }

    // Timer callback
    virtual void timerExpired ( Timer *timer ) override
    {
    }

    // Netplay constructor
    Main ( const IpAddrPort& address )
        : CommonMain ( address.addr.empty() ? ClientType::Host : ClientType::Client )
        , pinger ( new Pinger ( this, PING_INTERVAL, NUM_PINGS ) )
    {
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

    // Broadcast or offline constructor
    Main ( const NetplaySetup& netplaySetup )
        : CommonMain ( netplaySetup.broadcastPort ? ClientType::Broadcast : ClientType::Offline )
        , netplaySetup ( netplaySetup )
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        // Open the game wait and for callback to ipcConnectEvent
        procMan.openGame();
    }

    // Destructor
    virtual ~Main()
    {
        procMan.closeGame();

        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }
};


struct DummyMain : public Main
{
    PlayerInputs fakeInputs;

    void startGame() override
    {
        // Don't start the game in dummy mode
    }

    // Socket callback
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::CharaSelectLoaded:
                LOG ( "Character select loaded for both sides" );

                // Pretend we also just loaded character select
                ctrlSocket->send ( new CharaSelectLoaded() );

                // Now we can re-enable keepAlive
                ctrlSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
                dataSocket->setKeepAlive ( DEFAULT_KEEP_ALIVE );
                break;

            case MsgType::PlayerInputs:
                // Reply with fake inputs
                fakeInputs.frame = msg->getAs<PlayerInputs>().frame + netplaySetup.delay * 2;
                fakeInputs.index = msg->getAs<PlayerInputs>().index;
                fakeInputs.invalidate();
                dataSocket->send ( REF_PTR ( fakeInputs ) );
                break;

            default:
                Main::readEvent ( socket, msg, address );
                break;
        }
    }

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
    }

    // Constructor
    DummyMain ( const IpAddrPort& address ) : Main ( address ), fakeInputs ( 0, 0 )
    {
        fakeInputs.inputs.fill ( 0 );
    }
};


static void runMain ( const string& address, const NetplaySetup& netplaySetup )
{
    try
    {
        if ( address.empty() )
        {
            Main main ( netplaySetup );
            EventManager::get().start();
        }
        else
        {
            Main main ( address );
            EventManager::get().start();
        }
    }
    catch ( const Exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err );
    }
    catch ( const std::exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err.what() );
    }
    catch ( ... )
    {
        ui.sessionError = "Unknown error!";
    }
}


static void runDummy ( const string& address, const NetplaySetup& netplaySetup )
{
    ASSERT ( address.empty() != false );

    try
    {
        DummyMain main ( address );
        EventManager::get().start();
    }
    catch ( const Exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err );
    }
    catch ( const std::exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err.what() );
    }
    catch ( ... )
    {
        ui.sessionError = "Unknown error!";
    }
}


static Mutex deinitMutex;
static bool deinitialized = false;

static void deinitialize()
{
    LOCK ( deinitMutex );

    if ( deinitialized )
        return;
    deinitialized = true;

    EventManager::get().release();
    Logger::get().deinitialize();
    exit ( 0 );
}

static void signalHandler ( int signum )
{
    LOG ( "Interupt signal %d received", signum );
    deinitialize();
}

static BOOL WINAPI consoleCtrl ( DWORD ctrl )
{
    LOG ( "Console ctrl %d received", ctrl );
    deinitialize();
    return TRUE;
}


int main ( int argc, char *argv[] )
{
#if 0
    Logger::get().initialize();

    size_t pos = 0, consumed;
    char bytes[] =
    {
    };

    for ( ;; )
    {
        if ( pos == sizeof ( bytes ) )
            break;

        MsgPtr msg = Protocol::decode ( &bytes[pos], sizeof ( bytes ) - pos, consumed );

        pos += consumed;

        PRINT ( "%s", msg );

        if ( !msg )
            break;
    }

    Logger::get().deinitialize();
    return 0;
#endif

    static const Descriptor options[] =
    {
        { UNKNOWN, 0,  "",        "", Arg::None, "Usage: " BINARY " [options]\n\nOptions:" },
        { HELP,    0, "h",    "help", Arg::None, "  --help, -h    Print usage and exit." },
        { DUMMY,   0,  "",   "dummy", Arg::None, "  --dummy       Run as a dummy application." },
        { GTEST,   0,  "",   "gtest", Arg::None, "  --gtest       Run unit tests and exit." },
        { STDOUT,  0,  "",  "stdout", Arg::None, "  --stdout      Output logs to stdout." },
        { NO_FORK, 0,  "", "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie already running wineconosle
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

    // Setup signal and console handlers
    signal ( SIGABRT, signalHandler );
    signal ( SIGINT, signalHandler );
    signal ( SIGTERM, signalHandler );
    SetConsoleCtrlHandler ( consoleCtrl, TRUE );

    // Run the unit test suite
    if ( opt[GTEST] )
    {
        Logger::get().initialize();
        int result = RunAllTests ( argc, argv );
        Logger::get().deinitialize();
        return result;
    }

    // Fork and re-run under wineconsole, needed for proper JLib support
    if ( detectWine() && !opt[NO_FORK] )
        return system ( ( string ( "wineconsole " ) + GetCommandLine() + " --no-fork" ).c_str() );

    // Check if we should use stdout
    if ( opt[STDOUT] )
        Logger::get().initialize();
    else
        Logger::get().initialize ( LOG_FILE );

    // Check if we should run in dummy mode
    RunFuncPtr run = ( opt[DUMMY] ? runDummy : runMain );

    // Warn on invalid command line options
    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        ui.sessionMessage += toString ( "Unknown option: '%s'\n", it->name );

    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        ui.sessionMessage += toString ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    // Non-options 1 and 2 are the IP address and port
    if ( parser.nonOptionsCount() == 1 )
        run ( parser.nonOption ( 0 ), NetplaySetup() );
    else if ( parser.nonOptionsCount() == 2 )
        run ( string ( parser.nonOption ( 0 ) ) + parser.nonOption ( 1 ), NetplaySetup() );

    try
    {
        ui.main ( run );
    }
    catch ( const Exception& err )
    {
        PRINT ( "Error: %s", err );
    }
    catch ( const std::exception& err )
    {
        PRINT ( "Error: %s", err.what() );
    }
    catch ( ... )
    {
        PRINT ( "Unknown error!" );
    }

    deinitialize();
    return 0;
}


// Empty definition for unused DLL callback
extern "C" void callback() {}
