#include "Main.h"
#include "MainUi.h"
#include "Pinger.h"
#include "ExternalIpAddress.h"
#include "UdpSocket.h"
#include "SmartSocket.h"
#include "Constants.h"
#include "Logger.h"

#include <windows.h>
#include <ws2tcpip.h>

using namespace std;

#define PING_INTERVAL ( 1000/60 )

#define NUM_PINGS ( 10 )


extern vector<option::Option> opt;

extern MainUi ui;

extern string lastError;

extern string appDir;


struct MainApp
        : public Main
        , public Pinger::Owner
        , public ExternalIpAddress::Owner
        , public KeyboardManager::Owner
        , public Thread
{
    ExternalIpAddress externaIpAddress;

    InitialConfig initialConfig;

    bool isInitialConfigReady = false;

    SpectateConfig spectateConfig;

    NetplayConfig netplayConfig;

    Pinger pinger;

    PingStats pingStats;

    bool isBroadcastPortReady = false;

    bool isFinalConfigReady = false;

    bool isWaitingForUser = false;

    bool userConfirmed = false;

    Mutex uiMutex;

    CondVar uiCondVar;

    SocketPtr uiSendSocket, uiRecvSocket;

    bool isQueueing = false;

    vector<MsgPtr> msgQueue;

    bool isDummyReady = false;

    TimerPtr startTimer;

    IndexedFrame dummyFrame = {{ 0, 0 }};

    /* Connect protocol

        1 - Connect / accept ctrlSocket

        2 - Both send and recv VersionConfig

        3 - Both send and recv InitialConfig, then repeat to update names

        4 - Connect / accept dataSocket

        5 - Host pings, then sends PingStats

        6 - Client waits for PingStats, then pings, then sends PingStats

        7 - Both merge PingStats and wait for user confirmation

        8 - Host sends NetplayConfig and waits for ConfirmConfig before starting

        9 - Client confirms NetplayConfig and sends ConfirmConfig before starting

       10 - Reconnect dataSocket in-game, and also don't need ctrlSocket for host-client communications

       Spectate protocol

        1 - Connect / accept ctrlSocket

        2 - Both send and recv VersionConfig

        3 - Host sends SpectateConfig

        4 - Client recvs SpectateConfig and waits for user confirmation

    */

    void run() override
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

        stop();
    }

    void startNetplay()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        if ( clientMode.isHost() )
        {
            externaIpAddress.start();
            updateStatusMessage();
        }
        else
        {
            if ( options[Options::Tunnel] )
                ui.display ( toString ( "Trying %s (UDP tunnel)", address ) );
            else
                ui.display ( toString ( "Trying %s", address ) );
        }

        if ( clientMode.isHost() )
        {
            serverCtrlSocket = SmartSocket::listenTCP ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0

            LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );
        }
        else
        {
            ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );

            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );
        }

        EventManager::get().start();
    }

    void startSpectate()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        ui.display ( toString ( "Trying %s", address ) );

        ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );

        LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

        EventManager::get().start();
    }

    void startLocal()
    {
        AutoManager _;

        if ( clientMode.isBroadcast() )
        {
            externaIpAddress.start();
        }

        // Open the game immediately
        startGame();

        EventManager::get().start();
    }

    void stop()
    {
        EventManager::get().stop();
        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    void forwardMsgQueue()
    {
        if ( !procMan.isConnected() || msgQueue.empty() )
            return;

        for ( const MsgPtr& msg : msgQueue )
            procMan.ipcSend ( msg );

        msgQueue.clear();
    }

    void gotVersionConfig ( Socket *socket, const VersionConfig& versionConfig )
    {
        const Version RemoteVersion = versionConfig.version;

        LOG ( "LocalVersion='%s'; commitId='%s'; buildTime='%s'",
              LocalVersion, LocalVersion.commitId, LocalVersion.buildTime );

        LOG ( "RemoteVersion='%s'; commitId='%s'; buildTime='%s'",
              RemoteVersion, RemoteVersion.commitId, RemoteVersion.buildTime );

        LOG ( "VersionConfig: mode=%s; flags={ %s }", versionConfig.mode, versionConfig.mode.flagString() );

        if ( !LocalVersion.similar ( RemoteVersion, 1 + options[Options::StrictVersion] ) )
        {
            string local = toString ( "%s.%s", LocalVersion.major(), LocalVersion.minor() );
            string remote = toString ( "%s.%s", RemoteVersion.major(), RemoteVersion.minor() );

            if ( options[Options::StrictVersion] >= 1 )
            {
                local += LocalVersion.suffix();
                remote += RemoteVersion.suffix();
            }

            if ( options[Options::StrictVersion] >= 2 )
            {
                local += " " + LocalVersion.commitId;
                remote += " " + RemoteVersion.commitId;
            }

            if ( options[Options::StrictVersion] >= 3 )
            {
                local += " " + LocalVersion.buildTime;
                remote += " " + RemoteVersion.buildTime;
            }

            socket->send ( new ErrorMessage ( "Incompatible versions:\n" + local + "\n" + remote ) );
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
            if ( versionConfig.mode.isSpectate() )
            {
                socket->send ( new ErrorMessage ( "Not in a game yet, cannot spectate!" ) );
                return;
            }

            ctrlSocket = popPendingSocket ( socket );

            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

            if ( !ctrlSocket.get() )
                return;

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            try { serverDataSocket = SmartSocket::listenUDP ( this, address.port ); }
            catch ( ... ) { serverDataSocket = SmartSocket::listenUDP ( this, 0 ); }

            initialConfig.dataPort = serverDataSocket->address.port;

            LOG ( "serverDataSocket=%08x", serverDataSocket.get() );
        }

        initialConfig.invalidate();
        ctrlSocket->send ( initialConfig );
    }

    void gotInitialConfig ( const InitialConfig& initialConfig )
    {
        if ( !isInitialConfigReady )
        {
            isInitialConfigReady = true;

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

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            dataSocket = SmartSocket::connectUDP ( this, { address.addr, this->initialConfig.dataPort },
                                                   ctrlSocket->getAsSmart().isTunnel() );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            ui.display ( this->initialConfig.getConnectMessage ( "Connecting" ) );
        }
    }

    void gotPingStats ( const PingStats& pingStats )
    {
        this->pingStats = pingStats;

        if ( clientMode.isHost() )
        {
            mergePingStats();
            getUserConfirmation();
        }
        else
        {
            pinger.start();
        }
    }

    void mergePingStats()
    {
        dataSocket.reset();
        serverDataSocket.reset();

        LOG ( "PingStats (local): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pinger.getStats().getMean(), pinger.getStats().getWorst(),
              pinger.getStats().getStdErr(), pinger.getStats().getStdDev(), pinger.getPacketLoss() );

        LOG ( "PingStats (remote): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );

        pingStats.latency.merge ( pinger.getStats() );
        pingStats.packetLoss = ( pingStats.packetLoss + pinger.getPacketLoss() ) / 2;

        LOG ( "PingStats (merged): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );
    }

    void gotSpectateConfig ( const SpectateConfig& spectateConfig )
    {
        if ( !clientMode.isSpectate() )
        {
            LOG ( "Unexpected 'SpectateConfig'" );
            return;
        }

        LOG ( "SpectateConfig: mode=%s; flags={ %s }; delay=%u; rollback=%u; names={ '%s', '%s' }"
              "chara={ %u, %u }; moon={ %c, %c }; color= { %d, %d }; state=%u",
              spectateConfig.mode, spectateConfig.mode.flagString(),
              spectateConfig.delay, spectateConfig.rollback, spectateConfig.names[0], spectateConfig.names[1],
              spectateConfig.chara[0], spectateConfig.chara[1], spectateConfig.moon[0], spectateConfig.moon[1],
              ( int ) spectateConfig.color[0], ( int ) spectateConfig.color[1], spectateConfig.stage );

        this->spectateConfig = spectateConfig;

        ui.spectate ( spectateConfig );

        getUserConfirmation();
    }

    void gotNetplayConfig ( const NetplayConfig& netplayConfig )
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
        this->netplayConfig.sessionId = netplayConfig.sessionId;

        isFinalConfigReady = true;

        ui.connected ( netplayConfig );

        getUserConfirmation();
    }

    void getUserConfirmation()
    {
        // Disable keyboard hooks for the UI
        KeyboardManager::get().unhook();

        uiRecvSocket = UdpSocket::bind ( this, 0 );
        uiSendSocket = UdpSocket::bind ( 0, { "127.0.0.1", uiRecvSocket->address.port } );
        isWaitingForUser = true;

        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    void waitForUserConfirmation()
    {
        LOCK ( uiMutex );
        uiCondVar.wait ( uiMutex );

        if ( !EventManager::get().isRunning() )
            return;

        switch ( clientMode.value )
        {
            case ClientMode::Host:
                userConfirmed = ui.accepted ( initialConfig, pingStats );
                break;

            case ClientMode::Client:
            case ClientMode::Spectate:
                userConfirmed = ui.confirm();
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }

        uiSendSocket->send ( NullMsg );
    }

    void gotUserConfirmation()
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
                isQueueing = true;
                ctrlSocket->send ( new ConfirmConfig() );
                startGame();
                break;

            case ClientMode::Host:
                KeyboardManager::get().hook ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } ); // Waiting again
                netplayConfig = ui.getNetplayConfig();
                netplayConfig.sessionId = generateSessionId();
                netplayConfig.invalidate();
                ctrlSocket->send ( netplayConfig );
                break;

            case ClientMode::Client:
                ctrlSocket->send ( new ConfirmConfig() );
                ASSERT ( isFinalConfigReady == true );
                startGame();
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }
    }

    void gotConfirmConfig()
    {
        if ( !userConfirmed )
        {
            LOG ( "Unexpected 'ConfirmConfig'" );
            return;
        }

        isFinalConfigReady = true;
        startGame();
    }

    void gotDummyMsg ( const MsgPtr& msg )
    {
        ASSERT ( options[Options::Dummy] );
        ASSERT ( isDummyReady == true );
        ASSERT ( msg.get() != 0 );

        switch ( msg->getMsgType() )
        {
            case MsgType::RngState:
                return;

            case MsgType::PlayerInputs:
            {
                // TODO log inputs to check sync
                MsgPtr clone = msg->clone();
                clone->getAs<PlayerInputs>().indexedFrame.parts.frame += netplayConfig.delay * 2;
                dataSocket->send ( clone );
                return;
            }

            case MsgType::BothInputs:
                return;

            case MsgType::InputsContainer16:
                return;

            case MsgType::ErrorMessage:
                lastError = msg->getAs<ErrorMessage>().error;
                stop();
                return;

            default:
                break;
        }

        LOG ( "Unexpected '%s'", msg );
    }

    void startGame()
    {
        if ( clientMode.isLocal() )
            options.set ( Options::SessionId, 1, generateSessionId() );
        else if ( clientMode.isSpectate() )
            options.set ( Options::SessionId, 1, spectateConfig.sessionId );
        else
            options.set ( Options::SessionId, 1, netplayConfig.sessionId );

        LOG ( "SessionId '%s'", options.arg ( Options::SessionId ) );

        if ( options[Options::Dummy] )
        {
            ASSERT ( clientMode.value == ClientMode::Client || clientMode.value == ClientMode::Spectate );

            ui.display ( "Dummy is ready", false ); // Don't replace last message

            isDummyReady = true;

            dataSocket = SmartSocket::connectUDP ( this, address, ctrlSocket->getAsSmart().isTunnel() );

            LOG ( "dataSocket=%08x", dataSocket.get() );

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT * 2 );

            syncLog.sessionId = netplayConfig.sessionId;
            syncLog.initialize ( appDir + SYNC_LOG_FILE, LOG_VERSION );
            return;
        }

        ui.display ( toString ( "Starting %s mode...", clientMode.isTraining() ? "training" : "versus" ),
                     !clientMode.isClient() ); // Don't replace last message if client

        if ( clientMode.isClient() && ctrlSocket->isSmart() && ctrlSocket->getAsSmart().isTunnel() )
            clientMode.flags |= ClientMode::UdpTunnel;

        if ( clientMode.isNetplay() )
            netplayConfig.mode.flags = initialConfig.mode.flags;

        // Start game (and disconnect sockets) after a small delay since the final configs are still in flight
        startTimer.reset ( new Timer ( this ) );
        startTimer->start ( 1000 );
    }

    // Pinger callbacks
    void sendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        if ( !dataSocket || !dataSocket->isConnected() )
        {
            lastError = "Disconnected!";
            stop();
            return;
        }

        ASSERT ( pinger == &this->pinger );

        dataSocket->send ( ping );
    }

    void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) override
    {
        ASSERT ( pinger == &this->pinger );

        ctrlSocket->send ( new PingStats ( stats, packetLoss ) );

        if ( clientMode.isClient() )
        {
            mergePingStats();
            ui.connected ( initialConfig, pingStats );
        }
    }

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override
    {
        LOG ( "acceptEvent ( %08x )", serverSocket );

        if ( serverSocket == serverCtrlSocket.get() )
        {
            LOG ( "serverCtrlSocket->accept ( this )" );

            SocketPtr newSocket = serverCtrlSocket->accept ( this );

            LOG ( "newSocket=%08x", newSocket.get() );

            ASSERT ( newSocket != 0 );
            ASSERT ( newSocket->isConnected() == true );

            newSocket->send ( new VersionConfig ( clientMode ) );

            pushPendingSocket ( newSocket );
        }
        else if ( serverSocket == serverDataSocket.get() && ctrlSocket && ctrlSocket->isConnected() && !dataSocket )
        {
            LOG ( "serverDataSocket->accept ( this )" );

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

    void connectEvent ( Socket *socket ) override
    {
        LOG ( "connectEvent ( %08x )", socket );

        if ( socket == ctrlSocket.get() )
        {
            LOG ( "ctrlSocket connected!" );

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            ctrlSocket->send ( new VersionConfig ( clientMode ) );
        }
        else if ( socket == dataSocket.get() )
        {
            LOG ( "dataSocket connected!" );

            ASSERT ( dataSocket.get() != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            stopTimer.reset();
        }
        else
        {
            ASSERT_IMPOSSIBLE;
        }
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "disconnectEvent ( %08x )", socket );

        if ( socket == ctrlSocket.get() || socket == dataSocket.get() )
        {
            if ( isDummyReady && stopTimer )
            {
                dataSocket = SmartSocket::connectUDP ( this, address );
                LOG ( "dataSocket=%08x", dataSocket.get() );
                return;
            }

            LOG ( "%s disconnected!", ( socket == ctrlSocket.get() ? "ctrlSocket" : "dataSocket" ) );

            if ( clientMode.isHost() && !isWaitingForUser )
            {
                resetHost();
                return;
            }

            if ( ! ( userConfirmed && isFinalConfigReady ) || isDummyReady )
            {
                if ( lastError.empty() )
                    lastError = ( isInitialConfigReady ? "Disconnected!" : "Timed out!" );

                stop();
            }
            return;
        }

        popPendingSocket ( socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
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
        else if ( isDummyReady )
        {
            gotDummyMsg ( msg );
            return;
        }
        else if ( ctrlSocket.get() != 0 )
        {
            if ( isQueueing )
            {
                msgQueue.push_back ( msg );
                forwardMsgQueue();
                return;
            }

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

        socket->send ( new ErrorMessage ( "Another client is currently connecting!" ) );

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    void switchedToUdpTunnel ( Socket *socket ) override
    {
        if ( socket != ctrlSocket.get() )
            return;

        ui.display ( toString ( "Trying %s (UDP tunnel)", address ) );
    }

    // ProcessManager callbacks
    void ipcConnectEvent() override
    {
        ASSERT ( clientMode != ClientMode::Unknown );

        procMan.ipcSend ( options );
        procMan.ipcSend ( ControllerManager::get().getMappings() );
        procMan.ipcSend ( clientMode );
        procMan.ipcSend ( new IpAddrPort ( address.getAddrInfo()->ai_addr ) );

        if ( clientMode.isSpectate() )
        {
            procMan.ipcSend ( spectateConfig );
            forwardMsgQueue();
            return;
        }

        ASSERT ( netplayConfig.delay != 0xFF );

        netplayConfig.setNames ( initialConfig.localName, initialConfig.remoteName );
        netplayConfig.invalidate();

        procMan.ipcSend ( netplayConfig );

        ui.display ( toString ( "%s mode started", clientMode.isTraining() ? "Training" : "Versus" ) );
    }

    void ipcDisconnectEvent() override
    {
        if ( lastError.empty() )
            lastError = "Game closed!";
        stop();
    }

    void ipcReadEvent ( const MsgPtr& msg ) override
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
                isBroadcastPortReady = true;
                updateStatusMessage();
                return;

            default:
                LOG ( "Unexpected ipcReadEvent ( '%s' )", msg );
                return;
        }
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

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
        if ( timer == stopTimer.get() )
        {
            stop();
        }
        else if ( timer == startTimer.get() )
        {
            startTimer.reset();

            // We must disconnect the sockets before the game process is created,
            // otherwise Windows say conflicting ports EVEN if they are created later.
            ctrlSocket.reset();
            serverCtrlSocket.reset();

            // Open the game and wait for callback to ipcConnectEvent
            procMan.openGame ( appDir, ui.getConfig().getInteger ( "highCpuPriority" ) );
        }
        else
        {
            expirePendingSocketTimer ( timer );
        }
    }

    // ExternalIpAddress callbacks
    void foundExternalIpAddress ( ExternalIpAddress *extIpAddr, const string& address ) override
    {
        LOG ( "External IP address: '%s'", address );
        updateStatusMessage();
    }

    void unknownExternalIpAddress ( ExternalIpAddress *extIpAddr ) override
    {
        LOG ( "Unknown external IP address!" );
        updateStatusMessage();
    }

    // KeyboardManager callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown ) override
    {
        if ( vkCode == VK_ESCAPE )
            stop();
    }

    // Constructor
    MainApp ( const IpAddrPort& addr, const Serializable& config )
        : Main ( config.getMsgType() == MsgType::InitialConfig
                 ? config.getAs<InitialConfig>().mode
                 : config.getAs<NetplayConfig>().mode )
        , externaIpAddress ( this )
    {
        LOG ( "clientMode=%s; flags={ %s }; address='%s'; config=%s",
              clientMode, clientMode.flagString(), addr, config.getMsgType() );

        options = opt;
        address = addr;

        if ( !appDir.empty() )
            options.set ( Options::AppDir, 1, appDir );

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
    ~MainApp()
    {
        join();
        procMan.closeGame();

        if ( !lastError.empty() )
        {
            LOG ( "lastError='%s'", lastError );
            ui.sessionError = lastError;
        }

        syncLog.deinitialize();
    }

private:

    // Update the UI status message
    void updateStatusMessage() const
    {
        if ( clientMode.isBroadcast() && !isBroadcastPortReady )
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

    // Reset hosting state
    void resetHost()
    {
        ASSERT ( clientMode.isHost() == true );

        LOG ( "Resetting host!" );

        ctrlSocket.reset();
        dataSocket.reset();
        serverDataSocket.reset();

        initialConfig.dataPort = 0;
        initialConfig.remoteName.clear();
        isInitialConfigReady = false;

        netplayConfig.clear();

        pinger.clear();
        pingStats.clear();

        uiSendSocket.reset();
        uiRecvSocket.reset();

        isBroadcastPortReady = isFinalConfigReady = isWaitingForUser = userConfirmed = false;
    }

    static string generateSessionId()
    {
        string sessionId;

        for ( int i = 0; i < 10; ++i )
        {
            sessionId += ( 'A' + ( rand() % 26 ) );
            sessionId += ( 'a' + ( rand() % 26 ) );
            sessionId += ( '0' + ( rand() % 10 ) );
        }

        return sessionId;
    }
};


void runMain ( const IpAddrPort& address, const Serializable& config )
{
    lastError.clear();

    MainApp main ( address, config );

    main.start();
    main.waitForUserConfirmation();
}

void runFake ( const IpAddrPort& address, const Serializable& config )
{
}
