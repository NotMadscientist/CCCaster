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

        9 - Client send ConfirmConfig and waits for NetplayConfig before starting

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
            ui.display ( toString ( "Connecting to %s", address ) );
        }

        if ( clientMode.isHost() )
        {
            serverCtrlSocket = SmartSocket::listenTCP ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0

            LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );
        }
        else
        {
            ctrlSocket = SmartSocket::connectTCP ( this, address );

            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );
        }

        EventManager::get().start();
    }

    void startSpectate()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        ui.display ( toString ( "Connecting to %s", address ) );

        ctrlSocket = SmartSocket::connectTCP ( this, address );

        LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

        EventManager::get().start();
    }

    void startLocal()
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

        if ( !LocalVersion.similar ( RemoteVersion, 1 + options[Options::Strict] ) )
        {
            string local = toString ( "%s.%s", LocalVersion.major(), LocalVersion.minor() );
            string remote = toString ( "%s.%s", RemoteVersion.major(), RemoteVersion.minor() );

            if ( options[Options::Strict] >= 1 )
            {
                local += LocalVersion.suffix();
                remote += RemoteVersion.suffix();
            }

            if ( options[Options::Strict] >= 2 )
            {
                local += " " + LocalVersion.commitId;
                remote += " " + RemoteVersion.commitId;
            }

            if ( options[Options::Strict] >= 3 )
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

    void gotSpectateConfig ( const SpectateConfig& spectateConfig )
    {
        LOG ( "SpectateConfig: mode=%s; flags={ %s }; delay=%u; rollback=%u; names={ '%s', '%s' }"
              "chara={ %u, %u }; moon={ %c, %c }; color= { %d, %d }; state=%u",
              spectateConfig.mode, spectateConfig.mode.flagString(),
              spectateConfig.delay, spectateConfig.rollback, spectateConfig.names[0], spectateConfig.names[1],
              spectateConfig.chara[0], spectateConfig.chara[1], spectateConfig.moon[0], spectateConfig.moon[1],
              ( int ) spectateConfig.color[0], ( int ) spectateConfig.color[1], spectateConfig.stage );

        this->spectateConfig = spectateConfig;

        getUserConfirmation();
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
            getUserConfirmation();
        else
            pinger.start();
    }

    void getUserConfirmation()
    {
        dataSocket.reset();
        serverDataSocket.reset();

        // Disable keyboard hooks for the UI
        KeyboardManager::get().unhook();

        if ( clientMode.isNetplay() )
        {
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

        isFinalConfigReady = true;
        startGameIfReady();
    }

    void gotConfirmConfig()
    {
        isFinalConfigReady = true;
        startGameIfReady();
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
                // const PlayerInputs& remote = msg->getAs<PlayerInputs>();

                // if ( remote.getEndIndexedFrame().value < dummyFrame.value )
                //     return;

                // MsgPtr msgFakeInputs = remote.clone();
                // PlayerInputs& local = msgFakeInputs->getAs<PlayerInputs>();
                // local.invalidate();

                // ASSERT ( netplayConfig.hostPlayer == 1 || netplayConfig.hostPlayer == 2 );

                // // if ( dummyFrame.parts.index == remote.getIndex() )
                // {
                //     local.indexedFrame.parts.frame += 10;

                //     uint32_t start = dummyFrame.parts.frame;
                //     if ( local.getEndFrame() - dummyFrame.parts.frame > NUM_INPUTS )
                //         start = local.getStartFrame();
                //     if ( remote.getEndFrame() - dummyFrame.parts.frame > NUM_INPUTS )
                //         start = remote.getStartFrame();

                //     for ( uint32_t i = start; i < local.getEndFrame() && i < remote.getEndFrame(); ++i )
                //     {
                //         const uint32_t j = ( i - remote.getStartFrame() );

                //         // if ( netplayConfig.hostPlayer == 2 )
                //         //     LOG_TO ( syncLog, "Inputs: %04x %04x", local.inputs[j], remote.inputs[j] );
                //         // else
                //         //     LOG_TO ( syncLog, "Inputs: %04x %04x", remote.inputs[j], local.inputs[j] );
                //     }
                // }
                // // else
                // // {
                // //     for ( uint16_t& input : local.inputs )
                // //         input = 0;
                // // }

                // ASSERT ( dataSocket.get() != 0 );
                // ASSERT ( dataSocket->isConnected() == true );

                // dataSocket->send ( msgFakeInputs );

                // dummyFrame = remote.getEndIndexedFrame();
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

    void startGameIfReady()
    {
        if ( userConfirmed && isFinalConfigReady )
            startGame();
    }

    void startGame()
    {
        if ( options[Options::Dummy] )
        {
            ASSERT ( clientMode.value == ClientMode::Client || clientMode.value == ClientMode::Spectate );

            ui.display ( "Dummy is ready" );

            isDummyReady = true;

            dataSocket = SmartSocket::connectUDP ( this, address );
            LOG ( "dataSocket=%08x", dataSocket.get() );

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT * 2 );

            syncLog.initialize ( SYNC_LOG_FILE, 0 );
            return;
        }

        ui.display ( "Starting game..." );

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
            getUserConfirmation();
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

        ui.display ( toString ( "Connecting to %s (UDP tunnel)", address ) );
    }

    // ProcessManager callbacks
    void ipcConnectEvent() override
    {
        ASSERT ( clientMode != ClientMode::Unknown );

        procMan.ipcSend ( options );
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

        ui.display ( "Game started" );
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
            procMan.openGame();
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
    void keyboardEvent ( int vkCode, bool isDown ) override
    {
        if ( vkCode == VK_ESCAPE )
            stop();
    }

    // Constructor
    MainApp ( const IpAddrPort& address, const Serializable& config )
        : Main ( config.getMsgType() == MsgType::InitialConfig
                 ? config.getAs<InitialConfig>().mode
                 : config.getAs<NetplayConfig>().mode )
        , externaIpAddress ( this )
    {
        LOG ( "clientMode=%s; flags={ %s }; address='%s'; config=%s",
              clientMode, clientMode.flagString(), address, config.getMsgType() );

        this->options = opt;
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
};


void run ( const IpAddrPort& address, const Serializable& config )
{
    MainApp main ( address, config );
    main.start();
    main.waitForUserConfirmation();
}
