#include "Main.h"
#include "MainUi.h"
#include "Logger.h"
#include "Test.h"
#include "Constants.h"
#include "Pinger.h"
#include "ExternalIpAddress.h"
#include "KeyboardManager.h"

#include <optionparser.h>
#include <windows.h>

#include <exception>
#include <vector>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"

#define PING_INTERVAL ( 1000/60 )

#define NUM_PINGS ( 10 )

#define STOP_EVENTS_DELAY ( 1000 )


// Set of command line options
enum CommandLineOptions
{ UNKNOWN, HELP, DUMMY, UNIT_TEST, STDOUT, NO_FORK, NO_UI, STRICT_VERSION, TRAINING, BROADCAST, OFFLINE, DIRECTORY };

// Active command line options
static vector<Option> opt;

// Main UI instance
static MainUi ui;

// External IP address query tool
ExternalIpAddress externaIpAddress ( 0 );


struct Main
        : public CommonMain
        , public Pinger::Owner
        , public ExternalIpAddress::Owner
        , public KeyboardManager::Owner
{
    IpAddrPort address;

    InitialConfig initialConfig;

    NetplayConfig netplayConfig;

    Pinger pinger;

    PingStats pingStats;

    /* Connect procedure

        1 - Connect / accept ctrlSocket

        2 - Both send InitialConfig (flags, dataPort, version, name)

        3 - Both recv InitialConfig (update connecting message)

        4 - Connect / accept dataSocket

        5 - Host pings, then sends PingStats

        6 - Client waits for PingStats, then pings, then sends PingStats

        7 - Both merge PingStats and wait for user confirmation

        8 - Host sends NetplayConfig and waits for ConfirmConfig before starting

        9 - Client send ConfirmConfig and waits for NetplayConfig before starting

    */

    virtual void userConfirmation()
    {
        ASSERT ( ctrlSocket.get() != 0 );
        ASSERT ( dataSocket.get() != 0 );
        ASSERT ( ctrlSocket->isConnected() == true );
        ASSERT ( dataSocket->isConnected() == true );

        // Disable keepAlive because the UI blocks
        ctrlSocket->setKeepAlive ( 0 );
        dataSocket->setKeepAlive ( 0 );

        if ( isHost() )
        {
            serverCtrlSocket->setKeepAlive ( 0 );
            serverDataSocket->setKeepAlive ( 0 );
        }

        // Disable keyboard hooks for the UI
        KeyboardManager::get().unhook();

        pingStats.latency.merge ( pinger.getStats() );
        pingStats.packetLoss = ( pingStats.packetLoss + pinger.getPacketLoss() ) / 2;

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
            netplayConfig.invalidate();
            ctrlSocket->send ( REF_PTR ( netplayConfig ) );

            // Wait for ConfirmConfig before starting game
        }
        else
        {
            if ( !ui.connected ( initialConfig, pingStats ) )
            {
                EventManager::get().stop();
                return;
            }

            ctrlSocket->send ( new ConfirmConfig() );

            // Wait for NetplayConfig before starting game
        }
    }

    virtual void gotInitialConfig ( const InitialConfig& initialConfig )
    {
        this->initialConfig.remoteVersion = initialConfig.localVersion;
        this->initialConfig.remoteName = initialConfig.localName;

        const Version RemoteVersion = this->initialConfig.remoteVersion;

        LOG ( "Initial config:"
              "dataPort=%u; remoteVersion='%s'; commitId='%s'; buildTime='%s';"
              "remoteName='%s'; isTraining=%d; isBroadcast=%d",
              initialConfig.dataPort, RemoteVersion, RemoteVersion.commitId, RemoteVersion.buildTime,
              this->initialConfig.remoteName, initialConfig.isTraining(), initialConfig.isBroadcast() );

        if ( this->initialConfig.remoteName.empty() )
            this->initialConfig.remoteName = ctrlSocket->address.addr;

        if ( !LocalVersion.similar ( RemoteVersion, 1 + opt[STRICT_VERSION].count() ) )
        {
            string local = toString ( "%s.%s", LocalVersion.major(), LocalVersion.minor() );
            string remote = toString ( "%s.%s", RemoteVersion.major(), RemoteVersion.minor() );

            if ( opt[STRICT_VERSION].count() >= 1 )
            {
                local += LocalVersion.suffix();
                remote += RemoteVersion.suffix();
            }

            if ( opt[STRICT_VERSION].count() >= 2 )
            {
                local += " " + LocalVersion.commitId;
                remote += " " + RemoteVersion.commitId;
            }

            if ( opt[STRICT_VERSION].count() >= 3 )
            {
                local += " " + LocalVersion.buildTime;
                remote += " " + RemoteVersion.buildTime;
            }

            ui.sessionError = "Incompatible versions:\n" + local + "\n" + remote;
            ctrlSocket->send ( new ErrorMessage ( ui.sessionError ) );
            delayedStop();
            return;
        }

        if ( isHost() )
        {
            ui.display ( this->initialConfig.getAcceptMessage ( "connecting" ) );
        }
        else
        {
            this->initialConfig.flags = initialConfig.flags;
            this->initialConfig.dataPort = initialConfig.dataPort;

            dataSocket = UdpSocket::connect ( this, { address.addr, this->initialConfig.dataPort } );

            LOG ( "dataSocket=%08x", dataSocket );

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
            pinger.start();
    }

    virtual void startGame()
    {
        if ( isNetplay() )
        {
            netplayConfig.flags = initialConfig.flags;
            netplayConfig.invalidate();
        }

        // Remove sockets from the EventManager so messages get buffered by the OS while starting the game.
        // These are safe to call even if null or the socket is not a real fd.
        SocketManager::get().remove ( serverCtrlSocket.get() );
        SocketManager::get().remove ( serverDataSocket.get() );
        SocketManager::get().remove ( ctrlSocket.get() );
        SocketManager::get().remove ( dataSocket.get() );

        // Open the game wait and for callback to ipcConnectEvent
        procMan.openGame();
    }

    virtual void startNetplay()
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );
        // KeyboardManager::get().hook ( this, ui.getConsoleWindow(), { VK_ESCAPE } );

        if ( isHost() )
        {
            externaIpAddress.owner = this;
            externaIpAddress.start();
            updateStatusMessage();
        }
        else
        {
            ui.display ( toString ( "Connecting to %s", address ) );
        }

        if ( isHost() )
        {
            serverCtrlSocket = TcpSocket::listen ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0
        }
        else
        {
            ctrlSocket = TcpSocket::connect ( this, address );
            LOG ( "ctrlSocket=%08x", ctrlSocket );
        }

        EventManager::get().start();

        KeyboardManager::get().unhook();
        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }

    virtual void startLocal()
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( this );

        if ( isBroadcast() )
        {
            externaIpAddress.owner = this;
            externaIpAddress.start();
            updateStatusMessage();
        }

        // Open the game immediately
        procMan.openGame();

        EventManager::get().start();

        externaIpAddress.owner = 0;
        procMan.closeGame();

        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }

    virtual void delayedStop()
    {
        stopTimer.reset ( new Timer ( this ) );
        stopTimer->start ( STOP_EVENTS_DELAY );
    }

    // Pinger callbacks
    virtual void sendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        ASSERT ( pinger == &this->pinger );

        dataSocket->send ( ping );
    }

    virtual void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) override
    {
        ASSERT ( pinger == &this->pinger );

        LOG ( "Local stats: latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              stats.getMean(), stats.getWorst(), stats.getStdErr(), stats.getStdDev(), packetLoss );

        ctrlSocket->send ( new PingStats ( stats, packetLoss ) );

        if ( isClient() )
            userConfirmation();
    }

    // Socket callbacks
    virtual void acceptEvent ( Socket *serverSocket ) override
    {
        LOG ( "acceptEvent ( %08x )", serverSocket );

        if ( serverSocket == serverCtrlSocket.get() )
        {
            ctrlSocket = serverCtrlSocket->accept ( this );

            LOG ( "ctrlSocket=%08x", ctrlSocket );
            ASSERT ( ctrlSocket->isConnected() == true );

            serverDataSocket = UdpSocket::listen ( this, address.port ); // TODO choose a different port if UDP tunnel

            initialConfig.dataPort = serverDataSocket->address.port;
            initialConfig.invalidate();

            ctrlSocket->send ( REF_PTR ( initialConfig ) );
        }
        else if ( serverSocket == serverDataSocket.get() )
        {
            dataSocket = serverDataSocket->accept ( this );

            LOG ( "dataSocket=%08x", dataSocket );
            ASSERT ( dataSocket->isConnected() == true );

            pinger.start();
        }
        else
        {
            LOG ( "Unexpected acceptEvent from serverSocket=%08x", serverSocket );
            serverSocket->accept ( 0 ).reset();
            return;
        }
    }

    virtual void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );

        if ( socket == ctrlSocket.get() )
        {
            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            initialConfig.invalidate();

            ctrlSocket->send ( REF_PTR ( initialConfig ) );
        }
        else if ( socket == dataSocket.get() )
        {
            ASSERT ( dataSocket.get() != 0 );
            ASSERT ( dataSocket->isConnected() == true );
        }
        else
        {
            ASSERT_IMPOSSIBLE;
        }
    }

    virtual void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "disconnectEvent ( %08x )", socket );
        EventManager::get().stop();
    }

    virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "readEvent ( %08x, %s, %s )", socket, msg, address );

        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::InitialConfig:
                gotInitialConfig ( msg->getAs<InitialConfig>() );
                return;

            case MsgType::PingStats:
                gotPingStats ( msg->getAs<PingStats>() );
                return;

            case MsgType::NetplayConfig:
                netplayConfig = msg->getAs<NetplayConfig>(); // Intentional fall through to startGame

            case MsgType::ConfirmConfig:
                startGame();
                return;

            case MsgType::ErrorMessage:
                ui.sessionError = msg->getAs<ErrorMessage>().error;
                EventManager::get().stop();
                return;

            case MsgType::Ping:
                pinger.gotPong ( msg );
                return;

            default:
                break;
        }

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    // ProcessManager callbacks
    virtual void ipcConnectEvent() override
    {
        ASSERT ( clientMode != ClientMode::Unknown );
        ASSERT ( netplayConfig.delay != 0xFF );

        procMan.ipcSend ( REF_PTR ( clientMode ) );
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

                if ( ctrlSocket->isTCP() )
                {
                    procMan.ipcSend ( ctrlSocket->share ( procMan.getProcessId() ) );
                }
                else
                {
                    // We don't share child UDP sockets since they will be included in the server socket's share data
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
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::ErrorMessage:
                ui.sessionError = msg->getAs<ErrorMessage>().error;
                return;

            case MsgType::NetplayConfig:
                netplayConfig = msg->getAs<NetplayConfig>();
                updateStatusMessage();
                return;

            default:
                LOG ( "Unexpected ipcReadEvent ( '%s' )", msg );
                return;
        }
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

    // Timer callback
    virtual void timerExpired ( Timer *timer ) override
    {
        ASSERT ( timer == stopTimer.get() );

        EventManager::get().stop();
    }

    // ExternalIpAddress callbacks
    virtual void foundExternalIpAddress ( ExternalIpAddress *extIpAddr, const string& address ) override
    {
        LOG ( "External IP address: '%s'", address );
        updateStatusMessage();
    }

    virtual void unknownExternalIpAddress ( ExternalIpAddress *extIpAddr ) override
    {
        LOG ( "Unknown external IP address!" );
        updateStatusMessage();
    }

    // KeyboardManager callback
    virtual void keyboardEvent ( int vkCode, bool isDown ) override
    {
        if ( vkCode == VK_ESCAPE )
            EventManager::get().stop();
    }

    // Constructor
    Main ( const IpAddrPort& address, const Serializable& config )
        : CommonMain ( getClientMode ( address, config ) )
    {
        if ( isNetplay() )
        {
            this->address = address;
            this->initialConfig = config.getAs<InitialConfig>();

            pinger.owner = this;
            pinger.pingInterval = PING_INTERVAL;
            pinger.numPings = NUM_PINGS;
        }
        else if ( isLocal() )
        {
            netplayConfig = config.getAs<NetplayConfig>();
            initialConfig.flags = netplayConfig.flags; // For consistency
        }
        else
        {
            LOG_AND_THROW_IMPOSSIBLE;
        }
    }

    // Destructor
    virtual ~Main()
    {
        KeyboardManager::get().unhook();
        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }

private:

    // Determine the ClientMode from the address and config
    static ClientMode getClientMode ( const IpAddrPort& address, const Serializable& config )
    {
        if ( config.getMsgType() == MsgType::InitialConfig )
        {
            if ( address.addr.empty() )
                return ClientMode::Host;
            else
                return ClientMode::Client;
        }
        else if ( config.getMsgType() == MsgType::NetplayConfig )
        {
            if ( config.getAs<NetplayConfig>().isBroadcast() )
                return ClientMode::Broadcast;
            else
                return ClientMode::Offline;
        }
        else
        {
            LOG_AND_THROW_IMPOSSIBLE;
        }
    }

    // Update the UI status message
    void updateStatusMessage() const
    {
        const uint16_t port = ( isBroadcast() ? netplayConfig.broadcastPort : address.port );
        const ConfigOptions opt = ( isBroadcast() ? netplayConfig.flags : initialConfig.flags );

        if ( port == 0 && opt.isBroadcast() )
        {
            ui.display ( "Starting game..." );
        }
        else if ( externaIpAddress.address.empty() || externaIpAddress.address == "unknown" )
        {
            ui.display ( toString ( "%s on port %u%s\n",
                                    ( opt.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                    port,
                                    ( opt.isTraining() ? " (training mode)" : "" ) )
                         + ( externaIpAddress.address.empty()
                             ? "(Fetching external IP address...)"
                             : "(Could not find external IP address!)" ) );
        }
        else
        {
            setClipboard ( toString ( "%s:%u", externaIpAddress.address, port ) );

            ui.display ( toString ( "%s at %s:%u%s\n(Address copied to clipboard)",
                                    ( opt.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                    externaIpAddress.address,
                                    port,
                                    ( opt.isTraining() ? " (training mode)" : "" ) ) );
        }
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

    // Constructor
    DummyMain ( const IpAddrPort& address, const InitialConfig& initialConfig )
        : Main ( address, initialConfig ), fakeInputs ( { 0, 0 } )
    {
        fakeInputs.inputs.fill ( 0 );
    }
};


static void runMain ( const IpAddrPort& address, const Serializable& config )
{
    Main main ( address, config );

    try
    {
        if ( main.isNetplay() )
        {
            main.startNetplay();
        }
        else if ( main.isLocal() )
        {
            main.startLocal();
        }
        else
        {
            LOG_AND_THROW_IMPOSSIBLE;
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

    DummyMain main ( address, config.getAs<InitialConfig>() );

    try
    {
        main.startNetplay();
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

    externaIpAddress.stop();
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
        {
            UNKNOWN, 0, "", "", Arg::None,
            "Usage: " BINARY " [options] [address] [port]\n\nOptions:"
        },

        { HELP,           0, "h",      "help", Arg::None,     "  --help, -h         Print help and exit." },
        { DIRECTORY,      0, "d",       "dir", Arg::Required, "  --dir, -d folder   Specify path to game folder.\n" },

        { TRAINING,       0, "t",  "training", Arg::None,     "  --training, -t     Force training mode." },
        { BROADCAST,      0, "b", "broadcast", Arg::None,     "  --broadcast, -b    Force broadcast mode." },
        {
            OFFLINE, 0, "o", "offline", Arg::OptionalNumeric,
            "  --offline, -o [D]  Force offline mode.\n"
            "                     D is the delay, default 0.\n"
        },
        {
            NO_UI, 0, "n", "no-ui", Arg::None,
            "  --no-ui, -n        No UI, just quits after running once.\n"
            "                     Should be used with address and/or port.\n"
        },
        {
            STRICT_VERSION, 0, "s", "strict", Arg::None,
            "  --strict, -s       Strict version match, can be stacked up to 3 times.\n"
            "                     -s means version suffix must match.\n"
            "                     -ss means commit ID must match.\n"
            "                     -sss means build time must match.\n"
        },

        { STDOUT,    0, "",    "stdout", Arg::None, 0 }, // Output logs to stdout
        { UNIT_TEST, 0, "", "unit-test", Arg::None, 0 }, // Run unit tests and exit
        { DUMMY,     0, "",     "dummy", Arg::None, 0 }, // Client mode with fake inputs
        { NO_FORK,   0, "",   "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie running wineconsole

        {
            UNKNOWN, 0, "", "", Arg::None,
            "Examples:\n"
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
    Option buffer[stats.buffer_max];
    opt.resize ( stats.options_max );
    Parser parser ( options, argc, argv, &opt[0], buffer );

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
    if ( opt[UNIT_TEST] )
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

    if ( opt[DIRECTORY] && opt[DIRECTORY].arg )
        ProcessManager::gameDir = opt[DIRECTORY].arg;

    // Initialize config
    ui.initialize();
    ui.initialConfig.localVersion = LocalVersion;
    ui.initialConfig.flags |= ( opt[TRAINING] ? ConfigOptions::Training : 0 );
    ui.initialConfig.flags |= ( opt[BROADCAST] ? ConfigOptions::Broadcast : 0 );

    // Check if we should run in dummy mode
    RunFuncPtr run = ( opt[DUMMY] ? runDummy : runMain );

    // Warn on invalid command line options
    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        ui.sessionMessage += toString ( "Unknown option: '%s'\n", it->name );

    // Non-options 1 and 2 are the IP address and port
    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        ui.sessionMessage += toString ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    if ( opt[OFFLINE] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.flags = ( ConfigOptions::Offline | ( opt[TRAINING] ? ConfigOptions::Training : 0 ) );
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;

        if ( opt[OFFLINE].arg )
        {
            uint32_t delay = 0;
            stringstream ss ( opt[OFFLINE].arg );
            if ( ( ss >> delay ) && ( delay < 0xFF ) )
                netplayConfig.delay = delay;
        }

        run ( "", netplayConfig );
    }
    else if ( opt[BROADCAST] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.flags = ( ConfigOptions::Broadcast | ( opt[TRAINING] ? ConfigOptions::Training : 0 ) );
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;

        stringstream ss;
        if ( parser.nonOptionsCount() == 1 )
            ss << parser.nonOption ( 0 );
        else if ( parser.nonOptionsCount() == 2 )
            ss << parser.nonOption ( 1 );

        if ( ! ( ss >> netplayConfig.broadcastPort ) )
            netplayConfig.broadcastPort = 0;

        run ( "", netplayConfig );
    }
    else if ( parser.nonOptionsCount() == 1 )
    {
        run ( parser.nonOption ( 0 ), ui.initialConfig );
    }
    else if ( parser.nonOptionsCount() == 2 )
    {
        run ( string ( parser.nonOption ( 0 ) ) + ":" + parser.nonOption ( 1 ), ui.initialConfig );
    }
    else if ( opt[NO_UI] )
    {
        printUsage ( cout, options );
        return 0;
    }

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
