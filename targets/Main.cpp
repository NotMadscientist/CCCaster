#include "Main.h"
#include "MainUi.h"
#include "Logger.h"
#include "Test.h"
#include "Constants.h"
#include "Pinger.h"
#include "ExternalIpAddress.h"

#include <optionparser.h>
#include <windows.h>

#include <exception>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"

#define PING_INTERVAL ( 100 )

#define NUM_PINGS ( 5 )


// Set of command line options
enum CommandLineOptions { UNKNOWN, HELP, DUMMY, GTEST, STDOUT, NO_FORK, NO_UI, PLUS, TRAINING };

// Main UI instance
static MainUi ui;

// External IP address query tool
ExternalIpAddress externaIpAddress ( 0 );

// Update the status of the external IP address
static void updateExternalIpAddress ( uint16_t port, bool training );


/* Connect procedure (WIP)

    1 - Connect / accept sockets

    2 - Both send InitialConfig (displayName, isTraining, version TODO)

    3 - Both recv InitialConfig (update connecting message)

    4 - Host pings, then sends PingStats

    5 - Client waits for PingStats, then pings, then sends PingStats

    6 - Both merge PingStats and wait for user confirmation

    7 - Host sends NetplayConfig on confirm

    8 - Client waits for NetplayConfig before starting

*/

struct Main : public CommonMain, public Pinger::Owner, public ExternalIpAddress::Owner
{
    shared_ptr<Pinger> pinger;

    InitialConfig initialConfig;

    PingStats pingStats;

    NetplayConfig netplayConfig;


    virtual void userConfirmation()
    {
        // Disable keepAlive during the initial limbo period
        ctrlSocket->setKeepAlive ( 0 );
        dataSocket->setKeepAlive ( 0 );

        if ( isHost() )
        {
            serverCtrlSocket->setKeepAlive ( 0 );
            serverDataSocket->setKeepAlive ( 0 );
        }

        pingStats.latency.merge ( pinger->getStats() );
        pingStats.packetLoss = ( pingStats.packetLoss + pinger->getPacketLoss() ) / 200;

        LOG ( "Merged stats: latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );

        if ( isHost() )
        {
            if ( !ui.accepted ( initialConfig, pingStats ) )
            {
                EventManager::get().stop();
                return;
            }

            netplayConfig = ui.getNetplayConfig();

            // Make sure training mode is set consistently
            if ( initialConfig.isTraining )
            {
                netplayConfig.flags |= NetplayConfig::Training;
                netplayConfig.invalidate();
            }

            ctrlSocket->send ( REF_PTR ( netplayConfig ) );

            // Start the game and wait for callback to ipcConnectEvent
            startGame();
        }
        else
        {
            if ( !ui.connected ( initialConfig, pingStats ) )
            {
                EventManager::get().stop();
                return;
            }

            // Wait for NetplayConfig before starting game
        }
    }

    virtual void gotInitialConfig ( const InitialConfig& initialConfig )
    {
        LOG ( "Initial config: remoteName='%s'; training=%d", initialConfig.localName, initialConfig.isTraining );

        this->initialConfig.remoteName = initialConfig.localName;
        if ( this->initialConfig.remoteName.empty() )
            this->initialConfig.remoteName = ctrlSocket->address.addr;

        // TODO check versions here

        if ( isHost() )
        {
            ui.display ( this->initialConfig.getAcceptMessage ( "connecting" ) );
            pinger->start();
        }
        else
        {
            this->initialConfig.isTraining = initialConfig.isTraining;
            ui.display ( this->initialConfig.getConnectMessage ( "Connecting" ) );
        }
    }

    virtual void gotPingStats ( const PingStats& pingStats )
    {
        LOG ( "Remote stats: latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );

        this->pingStats = pingStats;

        if ( isHost() )
            userConfirmation();
        else
            pinger->start();
    }

    virtual void gotNetplayConfig ( const NetplayConfig& netplayConfig )
    {
        ASSERT ( isClient() == true );

        this->netplayConfig = netplayConfig;

        // Make sure training mode is set consistently
        if ( initialConfig.isTraining )
        {
            this->netplayConfig.flags |= NetplayConfig::Training;
            this->netplayConfig.invalidate();
        }

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

        LOG ( "Local stats: latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              stats.getMean(), stats.getWorst(), stats.getStdErr(), stats.getStdDev(), packetLoss );

        ctrlSocket->send ( new PingStats ( stats, packetLoss ) );

        if ( isClient() )
            userConfirmation();
    }

    // ExternalIpAddress callbacks
    virtual void foundExternalIpAddress ( ExternalIpAddress *extIpAddr, const string& address ) override
    {
        LOG ( "External IP address: '%s'", address );

        updateExternalIpAddress ( serverCtrlSocket->address.port, initialConfig.isTraining );
    }

    virtual void unknownExternalIpAddress ( ExternalIpAddress *extIpAddr ) override
    {
        LOG ( "Unknown external IP address!" );

        updateExternalIpAddress ( serverCtrlSocket->address.port, initialConfig.isTraining );
    }

    // ProcessManager callbacks
    virtual void ipcConnectEvent() override
    {
        ASSERT ( clientType != ClientType::Unknown );
        ASSERT ( netplayConfig.delay != 0xFF );

        procMan.ipcSend ( REF_PTR ( clientType ) );
        procMan.ipcSend ( REF_PTR ( netplayConfig ) );

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
        LOG ( "acceptEvent ( %08x )", serverSocket );

        // TODO proper queueing of potential spectators
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

            initialConfig.invalidate();
            ctrlSocket->send ( REF_PTR ( initialConfig ) );

            // TOOD version check
        }
    }

    virtual void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );

        ASSERT ( ctrlSocket.get() != 0 );
        ASSERT ( dataSocket.get() != 0 );
        ASSERT ( socket == ctrlSocket.get() || socket == dataSocket.get() );

        if ( ctrlSocket->isConnected() && dataSocket->isConnected() )
        {
            initialConfig.invalidate();
            ctrlSocket->send ( REF_PTR ( initialConfig ) );
        }
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
            case MsgType::InitialConfig:
                gotInitialConfig ( msg->getAs<InitialConfig>() );
                break;

            case MsgType::Ping:
                pinger->gotPong ( msg );
                break;

            case MsgType::PingStats:
                gotPingStats ( msg->getAs<PingStats>() );
                break;

            case MsgType::NetplayConfig:
                gotNetplayConfig ( msg->getAs<NetplayConfig>() );
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
    Main ( const IpAddrPort& address, const InitialConfig& initialConfig )
        : CommonMain ( address.addr.empty() ? ClientType::Host : ClientType::Client )
        , pinger ( new Pinger ( this, PING_INTERVAL, NUM_PINGS ) )
        , initialConfig ( initialConfig )
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

#ifdef UDP_ONLY
        if ( isHost() )
        {
            serverCtrlSocket = UdpSocket::listen ( this, address.port );
            serverDataSocket = UdpSocket::listen ( this, address.port + 1 );
        }
        else
        {
            ctrlSocket = UdpSocket::connect ( this, address );
            dataSocket = UdpSocket::connect ( this, { address.addr, uint16_t ( address.port + 1 ) } );
        }
#else
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
#endif

        if ( isHost() )
        {
            externaIpAddress.owner = this;
            externaIpAddress.start();
        }
    }

    // Broadcast or offline constructor
    Main ( const NetplayConfig& netplayConfig, uint16_t port = 0 )
        : CommonMain ( netplayConfig.isOffline() ? ClientType::Offline : ClientType::Broadcast )
        , netplayConfig ( netplayConfig )
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        if ( isBroadcast() )
        {
            ASSERT ( port != 0 );

            // TODO
        }

        // Open the game wait and for callback to ipcConnectEvent
        procMan.openGame();
    }

    // Destructor
    virtual ~Main()
    {
        externaIpAddress.owner = 0;

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
        // Don't start the game when in dummy mode
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
                fakeInputs.indexedFrame =
                {
                    msg->getAs<PlayerInputs>().getIndex(),
                    msg->getAs<PlayerInputs>().getFrame() + netplayConfig.delay * 2
                };
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
    DummyMain ( const IpAddrPort& address, const InitialConfig& initialConfig )
        : Main ( address, initialConfig ), fakeInputs ( { 0, 0 } )
    {
        fakeInputs.inputs.fill ( 0 );
    }
};


static void updateExternalIpAddress ( uint16_t port, bool training )
{
    if ( externaIpAddress.address.empty() || externaIpAddress.address == "unknown" )
    {
        ui.display ( toString ( "Hosting on port %u%s\n", port, ( training ? " (training mode)" : "" ) )
                     + ( externaIpAddress.address.empty()
                         ? "(Fetching external IP address...)"
                         : "(Could not find external IP address!)" ) );
    }
    else
    {
        setClipboard ( toString ( "%s:%u", externaIpAddress.address, port ) );

        ui.display ( toString ( "Hosting at %s:%u%s\n(Address copied to clipboard)", \
                                externaIpAddress.address, port, ( training ? " (training mode)" : "" ) ) );
    }
}


static void runMain ( const IpAddrPort& address, const Serializable& config )
{
    try
    {
        if ( config.getMsgType() == MsgType::NetplayConfig )
        {
            Main main ( config.getAs<NetplayConfig>(), address.port );
            EventManager::get().start();
        }
        else if ( config.getMsgType() == MsgType::InitialConfig )
        {
            if ( address.addr.empty() )
                updateExternalIpAddress ( address.port, config.getAs<InitialConfig>().isTraining );
            else
                ui.display ( toString ( "Connecting to %s", address ) );

            Main main ( address, config.getAs<InitialConfig>() );
            EventManager::get().start();
        }
        else
        {
            ASSERT ( !"This shouldn't happen!" );
        }
    }
    catch ( const Exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err );
    }
#ifdef NDEBUG
    catch ( const std::exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err.what() );
    }
    catch ( ... )
    {
        ui.sessionError = "Unknown error!";
    }
#endif
}


static void runDummy ( const IpAddrPort& address, const Serializable& config )
{
    ASSERT ( address.empty() == false );
    ASSERT ( address.addr.empty() == false );
    ASSERT ( config.getMsgType() == MsgType::InitialConfig );

    try
    {
        ui.display ( toString ( "Connecting to %s", address ) );
        DummyMain main ( address, config.getAs<InitialConfig>() );
        EventManager::get().start();
    }
    catch ( const Exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err );
    }
#ifdef NDEBUG
    catch ( const std::exception& err )
    {
        ui.sessionError = toString ( "Error: %s", err.what() );
    }
    catch ( ... )
    {
        ui.sessionError = "Unknown error!";
    }
#endif
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
    LOG ( "Interrupt signal %d received", signum );
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
    // Protocol testing code
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
        { UNKNOWN,  0,  "",         "", Arg::None, "Usage: " BINARY " [options] [address] [port]\n\nOptions:" },
        { TRAINING, 0, "t", "training", Arg::None, "  --training, -t  Training mode." },
        { HELP,     0, "h",     "help", Arg::None, "  --help, -h      Print usage and exit." },
        { DUMMY,    0,  "",    "dummy", Arg::None, "  --dummy         Run as a dummy application." },
        { GTEST,    0,  "",    "gtest", Arg::None, "  --gtest         Run unit tests and exit." },
        { STDOUT,   0,  "",   "stdout", Arg::None, "  --stdout        Output logs to stdout." },
        { NO_FORK,  0,  "",  "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie already running wineconosle
        { NO_UI,    0, "n",    "no-ui", Arg::None, "  --no-ui, -n     No UI, just quits after running once." },
        { PLUS,     0, "p",     "plus", Arg::None, "  --plus, -p      Increment count." },
        {
            UNKNOWN, 0, "", "", Arg::None,
            "\nExamples:\n"
            "  " BINARY " --unknown -- --this_is_no_option\n"
            "  " BINARY " -unk --plus -ppp file1 file2\n"
        },
        { 0, 0, 0, 0, 0, 0 }
    };

    string argv0;

    // Skip argv[0] if present, because optionparser doesn't like it
    if ( argc > 0 )
    {
        argv0 = argv[0];
        --argc;
        ++argv;
    }

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

    // Check if we should log to stdout
    if ( opt[STDOUT] )
        Logger::get().initialize();
    else
        Logger::get().initialize ( LOG_FILE );

    // Run the unit test suite and exit
    if ( opt[GTEST] )
    {
        int result = RunAllTests ( argc, argv );
        Logger::get().deinitialize();
        return result;
    }

    // Fork and re-run under wineconsole, needed for proper JLib support
    if ( detectWine() && !opt[NO_FORK] )
    {
        string cmd = "wineconsole " + argv0 + " --no-fork";

        for ( int i = 0; i < argc; ++i )
        {
            cmd += " ";
            cmd += argv[i];
        }

        PRINT ( "%s", cmd );
        return system ( cmd.c_str() );
    }

    // Check if we should run in dummy mode
    RunFuncPtr run = ( opt[DUMMY] ? runDummy : runMain );

    // Warn on invalid command line options
    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        ui.sessionMessage += toString ( "Unknown option: '%s'\n", it->name );

    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        ui.sessionMessage += toString ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    // Check if we are training mode
    if ( opt[TRAINING] )
        ui.initialConfig.isTraining = true;

    // Non-options 1 and 2 are the IP address and port
    if ( parser.nonOptionsCount() == 1 )
        run ( parser.nonOption ( 0 ), ui.initialConfig );
    else if ( parser.nonOptionsCount() == 2 )
        run ( string ( parser.nonOption ( 0 ) ) + ":" + parser.nonOption ( 1 ), ui.initialConfig );

    if ( !opt[NO_UI] )
    {
        try
        {
            ui.main ( run );
        }
        catch ( const Exception& err )
        {
            PRINT ( "Error: %s", err );
        }
#ifdef NDEBUG
        catch ( const std::exception& err )
        {
            PRINT ( "Error: %s", err.what() );
        }
        catch ( ... )
        {
            PRINT ( "Unknown error!" );
        }
#endif
    }

    deinitialize();
    return 0;
}


// Empty definition for unused DLL callback
extern "C" void callback() {}
