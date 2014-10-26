#include "Main.h"
#include "MainUi.h"
#include "Logger.h"
#include "Test.h"
#include "Constants.h"
#include "Pinger.h"
#include "ExternalIpAddress.h"

#include <optionparser.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <exception>
#include <vector>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"

#define PING_INTERVAL ( 1000/60 )

#define NUM_PINGS ( 10 )


// Set of command line options
enum CommandLineOptions
{
    UNKNOWN, HELP, DUMMY, UNIT_TESTS, STDOUT, NO_FORK, NO_UI, STRICT_VERSION,
    TRAINING, BROADCAST, SPECTATE, OFFLINE, DIRECTORY
};

// Active command line options
static vector<Option> opt;

// Main UI instance
static MainUi ui;

// The last error message
static string lastError;


struct Main
        : public CommonMain
        , public Pinger::Owner
        , public ExternalIpAddress::Owner
        , public KeyboardManager::Owner
        , public Thread
{
    ExternalIpAddress externaIpAddress;

    InitialConfig initialConfig;

    bool initialConfigReceived = false;

    SpectateConfig spectateConfig;

    NetplayConfig netplayConfig;

    Pinger pinger;

    PingStats pingStats;

    bool broadcastPortReady = false;

    bool finalConfigReceived = false;

    bool userConfirmed = false;

    Mutex uiMutex;

    CondVar uiCondVar;

    SocketPtr uiSendSocket, uiRecvSocket;

    /* Connect protocol

        1 - Connect / accept ctrlSocket

        2 - Both send and recv VersionConfig

        3 - Both send and recv InitialConfig, then repeat to update names

        4 - Connect / accept dataSocket

        5 - Host pings, then sends PingStats

        6 - Client waits for PingStats, then pings, then sends PingStats

        7 - Both merge PingStats and wait for user confirmation

        8 - Host sends NetplayConfig and waits for ConfirmConfig before starting

        9 - Client send ConfirmConfig and waits for NetplayConfig before starting

       10 - Reconnect dataSocket in-game, and also don't need ctrlSocket for host-client communications

       Spectate protocol

        1 - Connect / accept ctrlSocket

        2 - Both send and recv VersionConfig

        3 - Host sends SpectateConfig

        4 - Client recvs SpectateConfig and waits for user confirmation

    */

    virtual void run() override
    {
        try
        {
            if ( clientMode.isNetplay() )
            {
                startNetplay();
            }
            else if ( clientMode.isSpectate() )
            {
                startSpectate();
            }
            else if ( clientMode.isLocal() )
            {
                startLocal();
            }
            else
            {
                ASSERT_IMPOSSIBLE;
            }
        }
        catch ( const Exception& err )
        {
            lastError = toString ( "Error: %s", err );
        }
#ifdef NDEBUG
        catch ( const std::exception& err )
        {
            lastError = toString ( "Error: %s", err.what() );
        }
        catch ( ... )
        {
            lastError = "Unknown error!";
        }
#endif
    }

    virtual void startNetplay()
    {
        AutoManager _ ( this, ui.getConsoleWindow(), { VK_ESCAPE } );

        if ( clientMode.isHost() )
        {
            externaIpAddress.start();
            updateStatusMessage();
        }
        else
        {
            ui.display ( toString ( "Connecting to %s", address ) );
        }

        if ( clientMode.isHost() )
        {
            serverCtrlSocket = TcpSocket::listen ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0
            LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );
        }
        else
        {
            ctrlSocket = TcpSocket::connect ( this, address );
            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );
        }

        EventManager::get().start();
    }

    virtual void startSpectate()
    {
        AutoManager _ ( this, ui.getConsoleWindow(), { VK_ESCAPE } );

        ui.display ( toString ( "Connecting to %s", address ) );

        ctrlSocket = TcpSocket::connect ( this, address );

        LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

        EventManager::get().start();
    }

    virtual void startLocal()
    {
        AutoManager _ ( this );

        if ( clientMode.isBroadcast() )
        {
            externaIpAddress.start();
        }

        // Open the game immediately
        startGame();

        EventManager::get().start();
    }

    virtual void stop()
    {
        EventManager::get().stop();
        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    virtual void gotVersionConfig ( Socket *socket, const VersionConfig& versionConfig )
    {
        const Version RemoteVersion = versionConfig.version;

        LOG ( "LocalVersion='%s'; commitId='%s'; buildTime='%s'",
              LocalVersion, LocalVersion.commitId, LocalVersion.buildTime );

        LOG ( "RemoteVersion='%s'; commitId='%s'; buildTime='%s'",
              RemoteVersion, RemoteVersion.commitId, RemoteVersion.buildTime );

        LOG ( "VersionConfig: mode=%s; flags={ %s }", versionConfig.mode, versionConfig.mode.flagString() );

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

            lastError = "Incompatible versions:\n" + local + "\n" + remote;
            socket->send ( new ErrorMessage ( lastError ) );
            return;
        }

        // Switch to spectate mode if the game is already started
        if ( clientMode.isClient() && versionConfig.mode.isGameStarted() )
            clientMode.value = ClientMode::Spectate;

        if ( clientMode.isSpectate() )
        {
            if ( !versionConfig.mode.isGameStarted() )
            {
                lastError = "Not in a game yet, cannot spectate!";
                stop();
            }

            // Wait for SpectateConfig
            return;
        }

        if ( clientMode.isHost() )
        {
            if ( versionConfig.mode.isSpectate() ) // Ignore spectators, since the game is not loaded
                return;

            serverDataSocket = UdpSocket::listen ( this, address.port ); // TODO choose a different port if UDP tunnel
            initialConfig.dataPort = serverDataSocket->address.port;

            ctrlSocket = pendingSockets[socket];
            pendingSockets.erase ( socket );

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );
        }

        initialConfig.invalidate();
        ctrlSocket->send ( initialConfig );
    }

    virtual void gotSpectateConfig ( const SpectateConfig& spectateConfig )
    {
        LOG ( "SpectateConfig: mode=%s; flags={ %s }; delay=%u; rollback=%u; names={ '%s', '%s' }",
              spectateConfig.mode, spectateConfig.mode.flagString(),
              spectateConfig.delay, spectateConfig.rollback, spectateConfig.names[0], spectateConfig.names[1] );

        this->spectateConfig = spectateConfig;

        getUserConfirmation();
    }

    virtual void gotInitialConfig ( const InitialConfig& initialConfig )
    {
        if ( !initialConfigReceived )
        {
            initialConfigReceived = true;

            this->initialConfig.remoteName = initialConfig.localName;
            if ( this->initialConfig.remoteName.empty() )
                this->initialConfig.remoteName = ctrlSocket->address.addr;
            this->initialConfig.invalidate();

            ctrlSocket->send ( this->initialConfig );
            return;
        }

        // Update our real localName when we receive the 2nd InitialConfig
        this->initialConfig.localName = initialConfig.remoteName;

        LOG ( "InitialConfig: mode=%s; flags={ %s }; dataPort=%u; localName='%s'; remoteName='%s'",
              initialConfig.mode, initialConfig.mode.flagString(),
              initialConfig.dataPort, initialConfig.localName, initialConfig.remoteName );

        if ( clientMode.isClient() )
        {
            this->initialConfig.mode.flags = initialConfig.mode.flags;
            this->initialConfig.dataPort = initialConfig.dataPort;

            dataSocket = UdpSocket::connect ( this, { address.addr, this->initialConfig.dataPort } );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            ui.display ( this->initialConfig.getConnectMessage ( "Connecting" ) );
        }
    }

    virtual void gotPingStats ( const PingStats& pingStats )
    {
        LOG ( "PingStats (remote): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );

        this->pingStats = pingStats;

        if ( clientMode.isHost() )
            getUserConfirmation();
        else
            pinger.start();
    }

    virtual void getUserConfirmation()
    {
        // DllMain will reconnect the dataSockets
        dataSocket.reset();
        serverDataSocket.reset();

        // Disable keyboard hooks for the UI
        KeyboardManager::get().unhook();

        if ( clientMode.isNetplay() )
        {
            pingStats.latency.merge ( pinger.getStats() );
            pingStats.packetLoss = ( pingStats.packetLoss + pinger.getPacketLoss() ) / 2;

            LOG ( "PingStats (merged): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
                  pingStats.latency.getMean(), pingStats.latency.getWorst(),
                  pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );
        }

        uiRecvSocket = UdpSocket::bind ( this, 0 );
        uiSendSocket = UdpSocket::bind ( 0, { "127.0.0.1", uiRecvSocket->address.port } );

        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    virtual void waitForUserConfirmation()
    {
        LOCK ( uiMutex );
        uiCondVar.wait ( uiMutex );

        if ( !EventManager::get().isRunning() )
            return;

        switch ( clientMode.value )
        {
            case ClientMode::Spectate:
                userConfirmed = ui.spectate ( spectateConfig );
                break;

            case ClientMode::Host:
                userConfirmed = ui.accepted ( initialConfig, pingStats );
                break;

            case ClientMode::Client:
                userConfirmed = ui.connected ( initialConfig, pingStats );
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }

        uiSendSocket->send ( NullMsg );
    }

    virtual void gotUserConfirmation()
    {
        uiRecvSocket.reset();
        uiSendSocket.reset();

        if ( !userConfirmed || !ctrlSocket || !ctrlSocket->isConnected() )
        {
            if ( !ctrlSocket || !ctrlSocket->isConnected() )
                lastError = "Disconnected!";

            stop();
            return;
        }

        switch ( clientMode.value )
        {
            case ClientMode::Spectate:
                ctrlSocket->send ( new ConfirmConfig() );
                startGame();
                break;

            case ClientMode::Host:
                netplayConfig = ui.getNetplayConfig();
                netplayConfig.invalidate();

                ctrlSocket->send ( netplayConfig );

                ui.display ( "Waiting for client confirmation..." );
                startGameIfReady();
                break;

            case ClientMode::Client:
                ctrlSocket->send ( new ConfirmConfig() );

                ui.display ( "Waiting for host to choose delay..." );
                startGameIfReady();
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }
    }

    virtual void gotNetplayConfig ( const NetplayConfig& netplayConfig )
    {
        if ( !clientMode.isClient() )
        {
            LOG ( "Unexpected 'NetplayConfig'" );
            return;
        }

        this->netplayConfig.mode.flags = netplayConfig.mode.flags;
        this->netplayConfig.delay = netplayConfig.delay;
        this->netplayConfig.rollback = netplayConfig.rollback;
        this->netplayConfig.hostPlayer = netplayConfig.hostPlayer;

        finalConfigReceived = true;
        startGameIfReady();
    }

    virtual void gotConfirmConfig()
    {
        finalConfigReceived = true;
        startGameIfReady();
    }

    virtual void startGameIfReady()
    {
        if ( userConfirmed && finalConfigReceived )
            startGame();
    }

    virtual void startGame()
    {
        ui.display ( "Starting game..." );

        if ( clientMode.isNetplay() )
            netplayConfig.mode.flags = initialConfig.mode.flags;

        // Remove sockets from the EventManager so messages get buffered by the OS while starting the game.
        // These are safe to call even if null or the socket is not a real fd.
        SocketManager::get().remove ( serverCtrlSocket.get() );
        SocketManager::get().remove ( ctrlSocket.get() );

        // Open the game and wait for callback to ipcConnectEvent
        procMan.openGame();
    }

    // Pinger callbacks
    virtual void sendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        ASSERT ( pinger == &this->pinger );
        ASSERT ( dataSocket.get() != 0 );
        ASSERT ( dataSocket->isConnected() == true );

        dataSocket->send ( ping );
    }

    virtual void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) override
    {
        ASSERT ( pinger == &this->pinger );

        LOG ( "PingStats (local): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              stats.getMean(), stats.getWorst(), stats.getStdErr(), stats.getStdDev(), packetLoss );

        ctrlSocket->send ( new PingStats ( stats, packetLoss ) );

        if ( clientMode.isClient() )
            getUserConfirmation();
    }

    // Socket callbacks
    virtual void acceptEvent ( Socket *serverSocket ) override
    {
        LOG ( "acceptEvent ( %08x )", serverSocket );

        if ( serverSocket == serverCtrlSocket.get() )
        {
            SocketPtr newSocket = serverCtrlSocket->accept ( this );

            LOG ( "newSocket=%08x", newSocket.get() );

            ASSERT ( newSocket != 0 );
            ASSERT ( newSocket->isConnected() == true );

            newSocket->send ( new VersionConfig ( clientMode ) );

            pendingSockets[newSocket.get()] = newSocket;
        }
        else if ( serverSocket == serverDataSocket.get() )
        {
            dataSocket = serverDataSocket->accept ( this );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            ASSERT ( dataSocket != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            pinger.start();
        }
        else
        {
            LOG ( "Unexpected acceptEvent from serverSocket=%08x", serverSocket );
            serverSocket->accept ( 0 ).reset();
        }
    }

    virtual void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );

        if ( socket == ctrlSocket.get() )
        {
            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            ctrlSocket->send ( new VersionConfig ( clientMode ) );
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

        if ( socket == ctrlSocket.get() || socket == dataSocket.get() )
        {
            if ( socket == ctrlSocket.get() )
            {
                ctrlSocket.reset();
            }
            else if ( socket == dataSocket.get() )
            {
                dataSocket.reset();

                if ( !pinger.isPinging() )
                    return;
            }

            if ( clientMode.isHost() )
            {
                reset();
            }
            else
            {
                lastError = "Disconnected!";
                stop();
            }

            return;
        }

        pendingSockets.erase ( socket );
    }

    virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "readEvent ( %08x, %s, %s )", socket, msg, address );

        if ( socket == uiRecvSocket.get() && !msg.get() )
        {
            gotUserConfirmation();
            return;
        }

        if ( !msg.get() )
            return;

        if ( msg->getMsgType() == MsgType::VersionConfig
                && ( ( clientMode.isHost() && !ctrlSocket ) || clientMode.isClient() ) )
        {
            gotVersionConfig ( socket, msg->getAs<VersionConfig>() );
            return;
        }
        else if ( ctrlSocket )
        {
            switch ( msg->getMsgType() )
            {
                case MsgType::SpectateConfig:
                    gotSpectateConfig ( msg->getAs<SpectateConfig>() );
                    return;

                case MsgType::InitialConfig:
                    gotInitialConfig ( msg->getAs<InitialConfig>() );
                    return;

                case MsgType::PingStats:
                    gotPingStats ( msg->getAs<PingStats>() );
                    return;

                case MsgType::NetplayConfig:
                    gotNetplayConfig ( msg->getAs<NetplayConfig>() );
                    return;

                case MsgType::ConfirmConfig:
                    gotConfirmConfig();
                    return;

                case MsgType::ErrorMessage:
                    lastError = msg->getAs<ErrorMessage>().error;
                    stop();
                    return;

                case MsgType::Ping:
                    pinger.gotPong ( msg );
                    return;

                default:
                    break;
            }
        }

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    // ProcessManager callbacks
    virtual void ipcConnectEvent() override
    {
        ASSERT ( clientMode != ClientMode::Unknown );

        procMan.ipcSend ( clientMode );
        procMan.ipcSend ( new IpAddrPort ( address.getAddrInfo()->ai_addr ) );

        if ( clientMode.isSpectate() )
        {
            procMan.ipcSend ( spectateConfig );
            return;
        }

        ASSERT ( netplayConfig.delay != 0xFF );

        netplayConfig.setNames ( initialConfig.localName, initialConfig.remoteName );
        netplayConfig.invalidate();

        procMan.ipcSend ( netplayConfig );

        ui.display ( "Game started" );
    }

    virtual void ipcDisconnectEvent() override
    {
        lastError = "Game closed!";
        stop();
    }

    virtual void ipcReadEvent ( const MsgPtr& msg ) override
    {
        if ( !msg.get() )
            return;

        switch ( msg->getMsgType() )
        {
            case MsgType::ErrorMessage:
                lastError = msg->getAs<ErrorMessage>().error;
                return;

            case MsgType::NetplayConfig:
                netplayConfig = msg->getAs<NetplayConfig>();
                broadcastPortReady = true;
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
            stop();
    }

    // Constructor
    Main ( const IpAddrPort& address, const Serializable& config )
        : CommonMain ( config.getMsgType() == MsgType::InitialConfig
                       ? config.getAs<InitialConfig>().mode
                       : config.getAs<NetplayConfig>().mode )
        , externaIpAddress ( this )
    {
        LOG ( "clientMode=%s; flags={ %s }; address='%s'; config=%s",
              clientMode, clientMode.flagString(), address, config.getMsgType() );

        this->address = address;

        if ( clientMode.isNetplay() )
        {
            ASSERT ( config.getMsgType() == MsgType::InitialConfig );

            initialConfig = config.getAs<InitialConfig>();

            pinger.owner = this;
            pinger.pingInterval = PING_INTERVAL;
            pinger.numPings = NUM_PINGS;
        }
        else if ( clientMode.isSpectate() )
        {
            ASSERT ( config.getMsgType() == MsgType::InitialConfig );

            initialConfig = config.getAs<InitialConfig>();
        }
        else if ( clientMode.isLocal() )
        {
            ASSERT ( config.getMsgType() == MsgType::NetplayConfig );

            netplayConfig = config.getAs<NetplayConfig>();
        }
        else
        {
            ASSERT_IMPOSSIBLE;
        }
    }

    // Destructor
    virtual ~Main()
    {
        join();
        procMan.closeGame();

        if ( !userConfirmed )
            lastError.clear();

        if ( !lastError.empty() )
        {
            LOG ( "lastError='%s'", lastError );
            ui.sessionError = lastError;
        }
    }

private:

    // Update the UI status message
    void updateStatusMessage() const
    {
        if ( clientMode.isBroadcast() && !broadcastPortReady )
            return;

        const uint16_t port = ( clientMode.isBroadcast() ? netplayConfig.broadcastPort : address.port );

        if ( externaIpAddress.address.empty() || externaIpAddress.address == "unknown" )
        {
            ui.display ( toString ( "%s on port %u%s\n",
                                    ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                    port,
                                    ( clientMode.isTraining() ? " (training mode)" : "" ) )
                         + ( externaIpAddress.address.empty()
                             ? "(Fetching external IP address...)"
                             : "(Could not find external IP address!)" ) );
        }
        else
        {
            setClipboard ( toString ( "%s:%u", externaIpAddress.address, port ) );

            ui.display ( toString ( "%s at %s:%u%s\n(Address copied to clipboard)",
                                    ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                    externaIpAddress.address,
                                    port,
                                    ( clientMode.isTraining() ? " (training mode)" : "" ) ) );
        }
    }

    // Reset state
    void reset()
    {
        initialConfig.dataPort = 0;
        initialConfig.remoteName.clear();
        initialConfigReceived = false;
        netplayConfig.clear();
        pinger.stop();
        pingStats.clear();
        broadcastPortReady = false;
        finalConfigReceived = false;
        userConfirmed = false;
    }
};



static void runMain ( const IpAddrPort& address, const Serializable& config )
{
    Main main ( address, config );
    main.start();
    main.waitForUserConfirmation();
}


static void runDummy ( const IpAddrPort& address, const Serializable& config )
{
    ASSERT_UNIMPLEMENTED;
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
        {
            UNKNOWN, 0, "", "", Arg::None,
            "Usage: " BINARY " [options] [address] [port]\n\nOptions:"
        },

        { HELP,           0, "h",      "help", Arg::None,     "  --help, -h         Print help and exit." },
        { DIRECTORY,      0, "d",       "dir", Arg::Required, "  --dir, -d folder   Specify path to game folder.\n" },

        { TRAINING,       0, "t",  "training", Arg::None,     "  --training, -t     Force training mode." },
        { BROADCAST,      0, "b", "broadcast", Arg::None,     "  --broadcast, -b    Force broadcast mode." },
        { SPECTATE,       0, "s",  "spectate", Arg::None,     "  --spectate, -s     Force spectator mode." },
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
            STRICT_VERSION, 0, "S", "strict", Arg::None,
            "  --strict, -S       Strict version match, can be stacked up to 3 times.\n"
            "                     -S means version suffix must match.\n"
            "                     -SS means commit ID must match.\n"
            "                     -SSS means build time must match.\n"
        },

        { STDOUT,     0, "",     "stdout", Arg::None, 0 }, // Output logs to stdout
        { UNIT_TESTS, 0, "", "unit-tests", Arg::None, 0 }, // Run unit tests and exit
        { DUMMY,      0, "",      "dummy", Arg::None, 0 }, // Client mode with fake inputs
        { NO_FORK,    0, "",    "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie running wineconsole

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
    if ( opt[UNIT_TESTS] )
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
    ui.initialConfig.mode.flags |= ( opt[TRAINING] ? ClientMode::Training : 0 );

    if ( opt[SPECTATE] )
        ui.initialConfig.mode.value = ClientMode::Spectate;

    // Check if we should run in dummy mode
    RunFuncPtr run = ( opt[DUMMY] ? runDummy : runMain );

    // Warn on invalid command line options
    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        lastError += toString ( "Unknown option: '%s'\n", it->name );

    // Non-options 1 and 2 are the IP address and port
    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        lastError += toString ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    if ( opt[OFFLINE] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.mode.value = ClientMode::Offline;
        netplayConfig.mode.flags = ui.initialConfig.mode.flags;
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
        netplayConfig.mode.value = ClientMode::Broadcast;
        netplayConfig.mode.flags = ui.initialConfig.mode.flags;
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
        IpAddrPort address = parser.nonOption ( 0 );

        if ( ui.initialConfig.mode.value == ClientMode::Unknown )
            ui.initialConfig.mode.value = ( address.addr.empty() ? ClientMode::Host : ClientMode::Client );

        run ( address, ui.initialConfig );
    }
    else if ( parser.nonOptionsCount() == 2 )
    {
        IpAddrPort address = string ( parser.nonOption ( 0 ) ) + ":" + parser.nonOption ( 1 );

        if ( ui.initialConfig.mode.value == ClientMode::Unknown )
            ui.initialConfig.mode.value = ( address.addr.empty() ? ClientMode::Host : ClientMode::Client );

        run ( address, ui.initialConfig );
    }
    else if ( opt[NO_UI] )
    {
        printUsage ( cout, options );
        return 0;
    }

    if ( opt[NO_UI] )
    {
        if ( !lastError.empty() )
            PRINT ( "%s", lastError );
    }
    else
    {
        if ( !lastError.empty() )
            ui.sessionError = lastError;

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

    LOG ( "lastError='%s'", lastError );

    deinitialize();
    return 0;
}


// Empty definition for unused DLL callback
extern "C" void callback() {}
