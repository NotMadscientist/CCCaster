#include "Main.h"
#include "MainUi.h"
#include "Pinger.h"
#include "ExternalIpAddress.h"
#include "SmartSocket.h"
#include "UdpSocket.h"
#include "Constants.h"
#include "Exceptions.h"
#include "Algorithms.h"
#include "CharacterSelect.h"
#include "SpectatorManager.h"
#include "NetplayManager.h"

#include <windows.h>
#include <ws2tcpip.h>

using namespace std;

#define PING_INTERVAL ( 1000/60 )

#define NUM_PINGS ( 10 )


extern vector<option::Option> opt;

extern MainUi ui;

extern string lastError;

extern string appDir;

static Mutex uiMutex;

static CondVar uiCondVar;


// static string getClipboard()
// {
//     const char *buffer = "";

//     if ( OpenClipboard ( 0 ) )
//     {
//         HANDLE hData = GetClipboardData ( CF_TEXT );
//         buffer = ( const char * ) GlobalLock ( hData );
//         if ( buffer == 0 )
//             buffer = "";
//         GlobalUnlock ( hData );
//         CloseClipboard();
//     }
//     else
//     {
//         LOG ( "OpenClipboard failed: %s", WinException::getLastError() );
//     }

//     return string ( buffer );
// }

static void setClipboard ( const string& str )
{
    if ( OpenClipboard ( 0 ) )
    {
        HGLOBAL clipbuffer = GlobalAlloc ( GMEM_DDESHARE, str.size() + 1 );
        char *buffer = ( char * ) GlobalLock ( clipbuffer );
        strcpy ( buffer, LPCSTR ( str.c_str() ) );
        GlobalUnlock ( clipbuffer );
        EmptyClipboard();
        SetClipboardData ( CF_TEXT, clipbuffer );
        CloseClipboard();
    }
    else
    {
        LOG ( "OpenClipboard failed: %s", WinException::getLastError() );
    }
}

struct MainApp
        : public Main
        , public Pinger::Owner
        , public ExternalIpAddress::Owner
        , public KeyboardManager::Owner
        , public Thread
        , public SpectatorManager
{
    IpAddrPort originalAddress;

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

    SocketPtr uiSendSocket, uiRecvSocket;

    bool isQueueing = false;

    vector<MsgPtr> msgQueue;

    bool isDummyReady = false;

    TimerPtr startTimer;

    IndexedFrame dummyFrame = {{ 0, 0 }};

    bool delayChanged = false;

    bool rollbackChanged = false;

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
        catch ( const Exception& exc )
        {
            lastError = exc.user;
        }
#ifdef NDEBUG
        catch ( const std::exception& exc )
        {
            lastError = format ( "Error: %s", exc.what() );
        }
        catch ( ... )
        {
            lastError = "Unknown error!";
        }
#endif // NDEBUG

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
                ui.display ( format ( "Trying %s (UDP tunnel)", address ) );
            else
                ui.display ( format ( "Trying %s", address ) );
        }

        if ( clientMode.isHost() )
        {
            serverCtrlSocket = SmartSocket::listenTCP ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0
            address.invalidate();

            LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );
        }
        else
        {
            ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );
            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT );
        }

        EventManager::get().start();
    }

    void startSpectate()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        ui.display ( format ( "Trying %s", address ) );

        ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );
        LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

        EventManager::get().start();
    }

    void startLocal()
    {
        AutoManager _;

        if ( clientMode.isBroadcast() )
            externaIpAddress.start();

        // Open the game immediately
        startGame();

        EventManager::get().start();
    }

    void stop ( const string& error = "" )
    {
        if ( !error.empty() )
            lastError = error;

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

        LOG ( "LocalVersion='%s'; revision='%s'; buildTime='%s'",
              LocalVersion, LocalVersion.revision, LocalVersion.buildTime );

        LOG ( "RemoteVersion='%s'; revision='%s'; buildTime='%s'",
              RemoteVersion, RemoteVersion.revision, RemoteVersion.buildTime );

        LOG ( "VersionConfig: mode=%s; flags={ %s }", versionConfig.mode, versionConfig.mode.flagString() );

        if ( !LocalVersion.similar ( RemoteVersion, 1 + options[Options::StrictVersion] ) )
        {
            string local = LocalVersion.code;
            string remote = RemoteVersion.code;

            if ( options[Options::StrictVersion] >= 2 )
            {
                local += " " + LocalVersion.revision;
                remote += " " + RemoteVersion.revision;
            }

            if ( options[Options::StrictVersion] >= 3 )
            {
                local += " " + LocalVersion.buildTime;
                remote += " " + RemoteVersion.buildTime;
            }

            if ( clientMode.isHost() )
                socket->send ( new ErrorMessage ( "Incompatible host version: " + local ) );
            else
                stop ( "Incompatible host version: " + remote );
            return;
        }

        // Switch to spectate mode if the game is already started
        if ( clientMode.isClient() && versionConfig.mode.isGameStarted() )
            clientMode.value = ClientMode::SpectateNetplay;

        // Update spectate type
        if ( clientMode.isSpectate() && versionConfig.mode.isBroadcast() )
            clientMode.value = ClientMode::SpectateBroadcast;

        if ( clientMode.isSpectate() )
        {
            if ( !versionConfig.mode.isGameStarted() )
                stop ( "Not in a game yet, cannot spectate!" );

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

            this->initialConfig.mode.flags |= initialConfig.mode.flags;

            this->initialConfig.remoteName = initialConfig.localName;

            if ( this->initialConfig.remoteName.empty() )
                this->initialConfig.remoteName = ctrlSocket->address.addr;

            this->initialConfig.invalidate();

            ctrlSocket->send ( this->initialConfig );
            return;
        }

        // Update our real localName when we receive the 2nd InitialConfig
        this->initialConfig.localName = initialConfig.remoteName;

        if ( clientMode.isClient() )
        {
            this->initialConfig.mode.flags = initialConfig.mode.flags;
            this->initialConfig.dataPort = initialConfig.dataPort;
            this->initialConfig.winCount = initialConfig.winCount;

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            dataSocket = SmartSocket::connectUDP ( this, { address.addr, this->initialConfig.dataPort },
                                                   ctrlSocket->getAsSmart().isTunnel() );
            LOG ( "dataSocket=%08x", dataSocket.get() );

            ui.display (
                "Connecting to " + this->initialConfig.remoteName
                + "\n\n" + ( this->initialConfig.mode.isTraining() ? "Training" : "Versus" ) + " mode"
                + "\n\nCalculating delay..." );
        }

        LOG ( "InitialConfig: mode=%s; flags={ %s }; dataPort=%u; localName='%s'; remoteName='%s'; winCount=%u",
              initialConfig.mode, initialConfig.mode.flagString(),
              initialConfig.dataPort, initialConfig.localName, initialConfig.remoteName, initialConfig.winCount );
    }

    void gotPingStats ( const PingStats& pingStats )
    {
        this->pingStats = pingStats;

        if ( clientMode.isHost() )
        {
            mergePingStats();

            const int delay = computeDelay ( this->pingStats.latency.getMean() );
            const int maxDelay = ui.getConfig().getInteger ( "maxRealDelay" );

            if ( delay > maxDelay )
            {
                if ( ctrlSocket && ctrlSocket->isConnected() )
                {
                    string error = MainUi::formatStats ( this->pingStats )
                                   + format ( "\n\nNetwork delay greater than limit: %u", maxDelay );

                    ctrlSocket->send ( new ErrorMessage ( error ) );

                    pushPendingSocket ( this, ctrlSocket );
                }

                resetHost();
                return;
            }

            getUserConfirmation();
        }
        else
        {
            pinger.start();
        }
    }

    void mergePingStats()
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

    void gotSpectateConfig ( const SpectateConfig& spectateConfig )
    {
        if ( !clientMode.isSpectate() )
        {
            LOG ( "Unexpected 'SpectateConfig'" );
            return;
        }

        LOG ( "SpectateConfig: %s; flags={ %s }; delay=%d; rollback=%d; winCount=%d; hostPlayer=%u; "
              "names={ '%s', '%s' }", spectateConfig.mode, spectateConfig.mode.flagString(), spectateConfig.delay,
              spectateConfig.rollback, spectateConfig.winCount, spectateConfig.hostPlayer,
              spectateConfig.names[0], spectateConfig.names[1] );

        LOG ( "InitialGameState: %s; stage=%u; isTraining=%u; %s vs %s",
              NetplayState ( ( NetplayState::Enum ) spectateConfig.initial.netplayState ),
              spectateConfig.initial.stage, spectateConfig.initial.isTraining,
              spectateConfig.formatPlayer ( 1, getFullCharaName ),
              spectateConfig.formatPlayer ( 2, getFullCharaName ) );

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
        this->netplayConfig.rollbackDelay = netplayConfig.rollbackDelay;
        this->netplayConfig.winCount = netplayConfig.winCount;
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

        // Auto-confirm any settings if necessary
        if ( options[Options::Dummy] || options[Options::SyncTest] )
        {
            isWaitingForUser = true;
            userConfirmed = true;

            if ( clientMode.isHost() )
            {
                // TODO parse these from SyncTest arg
                netplayConfig.delay = computeDelay ( pingStats.latency.getWorst() ) + 1;
                netplayConfig.rollback = 4;
                netplayConfig.rollbackDelay = 0;
                netplayConfig.hostPlayer = 1;
                netplayConfig.sessionId = generateRandomId();
                netplayConfig.invalidate();

                ctrlSocket->send ( netplayConfig );

                gotConfirmConfig();
            }
            else
            {
                gotUserConfirmation();
            }
            return;
        }

        uiRecvSocket = UdpSocket::bind ( this, 0 );
        uiSendSocket = UdpSocket::bind ( 0, { "127.0.0.1", uiRecvSocket->address.port } );
        isWaitingForUser = true;

        // Unblock the thread waiting for user confirmation
        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    void waitForUserConfirmation()
    {
        // This runs a different thread waiting for user confirmation
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
            case ClientMode::SpectateNetplay:
            case ClientMode::SpectateBroadcast:
                userConfirmed = ui.confirm ( "Continue?" );
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }

        // Signal the main thread via a UDP packet
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
            case ClientMode::SpectateNetplay:
            case ClientMode::SpectateBroadcast:
                isQueueing = true;

                ctrlSocket->send ( new ConfirmConfig() );
                startGame();
                break;

            case ClientMode::Host:
                // Waiting again
                KeyboardManager::get().keyboardWindow = MainUi::getConsoleWindow();
                KeyboardManager::get().matchedKeys = { VK_ESCAPE };
                KeyboardManager::get().ignoredKeys.clear();
                KeyboardManager::get().hook ( this );

                netplayConfig = ui.getNetplayConfig();
                netplayConfig.sessionId = generateRandomId();
                netplayConfig.invalidate();

                ctrlSocket->send ( netplayConfig );
                break;

            case ClientMode::Client:
                ASSERT ( isFinalConfigReady == true );

                ctrlSocket->send ( new ConfirmConfig() );
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
            case MsgType::InitialGameState:
                LOG ( "InitialGameState: %s; stage=%u; isTraining=%u; %s vs %s",
                      NetplayState ( ( NetplayState::Enum ) msg->getAs<InitialGameState>().netplayState ),
                      msg->getAs<InitialGameState>().stage, msg->getAs<InitialGameState>().isTraining,
                      msg->getAs<InitialGameState>().formatCharaName ( 1, getFullCharaName ),
                      msg->getAs<InitialGameState>().formatCharaName ( 2, getFullCharaName ) );
                return;

            case MsgType::RngState:
                return;

            case MsgType::PlayerInputs:
            {
                // TODO log dummy inputs to check sync
                PlayerInputs inputs ( msg->getAs<PlayerInputs>().indexedFrame );
                inputs.indexedFrame.parts.frame += netplayConfig.delay * 2;

                for ( uint32_t i = 0; i < inputs.size(); ++i )
                {
                    const uint32_t frame = i + inputs.getStartFrame();
                    inputs.inputs[i] = ( ( frame % 5 ) ? 0 : COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM ) );
                }

                dataSocket->send ( inputs );
                return;
            }

            case MsgType::MenuIndex:
                // Dummy mode always chooses the first retry menu option,
                // since the higher option always takes priority, the host effectively takes priority.
                if ( clientMode.isClient() )
                    dataSocket->send ( new MenuIndex ( msg->getAs<MenuIndex>().index, 0 ) );
                return;

            case MsgType::BothInputs:
            {
                static IndexedFrame last = {{ 0, 0 }};

                const BothInputs& both = msg->getAs<BothInputs>();

                if ( both.getIndex() > last.parts.index )
                {
                    for ( uint32_t i = 0; i < both.getStartFrame(); ++i )
                        LOG_TO ( syncLog, "Dummy [%u:%u] Inputs: 0x%04x 0x%04x", both.getIndex(), i, 0, 0 );
                }

                for ( uint32_t i = 0; i < both.size(); ++i )
                {
                    const IndexedFrame current = {{ i + both.getStartFrame(), both.getIndex() }};

                    if ( current.value <= last.value )
                        continue;

                    LOG_TO ( syncLog, "Dummy [%s] Inputs: 0x%04x 0x%04x",
                             current, both.inputs[0][i], both.inputs[1][i] );
                }

                last = both.indexedFrame;
                return;
            }

            case MsgType::ErrorMessage:
                stop ( msg->getAs<ErrorMessage>().error );
                return;

            default:
                break;
        }

        LOG ( "Unexpected '%s'", msg );
    }

    void startGame()
    {
        KeyboardManager::get().unhook();

        if ( clientMode.isLocal() )
            options.set ( Options::SessionId, 1, generateRandomId() );
        else if ( clientMode.isSpectate() )
            options.set ( Options::SessionId, 1, spectateConfig.sessionId );
        else
            options.set ( Options::SessionId, 1, netplayConfig.sessionId );

        if ( clientMode.isClient() && ctrlSocket->isSmart() && ctrlSocket->getAsSmart().isTunnel() )
            clientMode.flags |= ClientMode::UdpTunnel;

        if ( clientMode.isNetplay() )
        {
            netplayConfig.mode.value = clientMode.value;
            netplayConfig.mode.flags = clientMode.flags = initialConfig.mode.flags;
            netplayConfig.winCount = initialConfig.winCount;
            netplayConfig.setNames ( initialConfig.localName, initialConfig.remoteName );

            LOG ( "NetplayConfig: %s; flags={ %s }; delay=%d; rollback=%d; rollbackDelay=%d; winCount=%d; "
                  "hostPlayer=%d; names={ '%s', '%s' }", netplayConfig.mode, netplayConfig.mode.flagString(),
                  netplayConfig.delay, netplayConfig.rollback, netplayConfig.rollbackDelay, netplayConfig.winCount,
                  netplayConfig.hostPlayer, netplayConfig.names[0], netplayConfig.names[1] );
        }

        if ( clientMode.isSpectate() )
            clientMode.flags = spectateConfig.mode.flags;

        LOG ( "SessionId '%s'", options.arg ( Options::SessionId ) );

        if ( options[Options::Dummy] )
        {
            ASSERT ( clientMode.value == ClientMode::Client || clientMode.isSpectate() == true );

            ui.display ( format ( "Dummy is ready%s", clientMode.isTraining() ? " (training)" : "" ),
                         false ); // Don't replace last message

            isDummyReady = true;

            // We need to send an IpAddrPort message to indicate our serverCtrlSocket address, here it is a fake
            if ( ctrlSocket && ctrlSocket->isConnected() )
                ctrlSocket->send ( NullAddress );

            // Only connect the dataSocket if isClient
            if ( clientMode.isClient() )
            {
                dataSocket = SmartSocket::connectUDP ( this, address, ctrlSocket->getAsSmart().isTunnel() );
                LOG ( "dataSocket=%08x", dataSocket.get() );
            }

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT * 2 );

            syncLog.sessionId = ( clientMode.isSpectate() ? spectateConfig.sessionId : netplayConfig.sessionId );

            if ( options[Options::PidLog] )
                syncLog.initialize ( appDir + SYNC_LOG_FILE, PID_IN_FILENAME );
            else
                syncLog.initialize ( appDir + SYNC_LOG_FILE, 0 );
            syncLog.logVersion();
            return;
        }

        ui.display ( format ( "Starting %s mode...", clientMode.isTraining() ? "training" : "versus" ),
                     ! ( clientMode.isClient() || clientMode.isSpectate() ) );
        // Replace last message if not client or spectator

        // Start game (and disconnect sockets) after a small delay since the final configs are still in flight
        startTimer.reset ( new Timer ( this ) );
        startTimer->start ( 1000 );
    }

    // Pinger callbacks
    void sendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        if ( !dataSocket || !dataSocket->isConnected() )
        {
            stop ( "Disconnected!" );
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

            pushPendingSocket ( this, newSocket );
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

            // TODO auto reconnect to original host address

            if ( socket == ctrlSocket.get() && clientMode.isSpectate() )
            {
                forwardMsgQueue();
                procMan.ipcSend ( new ErrorMessage ( "Disconnected!" ) );
                return;
            }

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

        stopTimer.reset();

        if ( msg->getMsgType() == MsgType::IpAddrPort && socket == ctrlSocket.get() && clientMode.isSpectate() )
        {
            this->address = msg->getAs<IpAddrPort>();
            ctrlSocket = SmartSocket::connectTCP ( this, this->address, options[Options::Tunnel] );
            return;
        }
        else if ( msg->getMsgType() == MsgType::VersionConfig
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
                    stop ( lastError = msg->getAs<ErrorMessage>().error );
                    return;

                case MsgType::Ping:
                    pinger.gotPong ( msg );
                    return;

                default:
                    break;
            }
        }

        if ( clientMode.isHost() && msg->getMsgType() == MsgType::VersionConfig )
        {
            if ( msg->getAs<VersionConfig>().mode.isSpectate() )
                socket->send ( new ErrorMessage ( "Not in a game yet, cannot spectate!" ) );
            else
                socket->send ( new ErrorMessage ( "Another client is currently connecting!" ) );
        }

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    void switchedToUdpTunnel ( Socket *socket ) override
    {
        if ( socket != ctrlSocket.get() )
            return;

        ui.display ( format ( "Trying %s (UDP tunnel)", address ) );
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

        netplayConfig.invalidate();

        procMan.ipcSend ( netplayConfig );

        ui.display ( format ( "Started %s mode", clientMode.isTraining() ? "training" : "versus" ) );
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
                stop ( msg->getAs<ErrorMessage>().error );
                return;

            case MsgType::NetplayConfig:
                netplayConfig = msg->getAs<NetplayConfig>();
                isBroadcastPortReady = true;
                updateStatusMessage();
                return;

            case MsgType::IpAddrPort:
                if ( ctrlSocket && ctrlSocket->isConnected() )
                    ctrlSocket->send ( msg );
                return;

            case MsgType::ChangeConfig:
                if ( msg->getAs<ChangeConfig>().value == ChangeConfig::Delay )
                    delayChanged = true;

                if ( msg->getAs<ChangeConfig>().value == ChangeConfig::Rollback )
                    rollbackChanged = true;

                if ( delayChanged && rollbackChanged )
                    ui.display ( format ( "Input delay was changed to %u\nRollback was changed to %u",
                                          msg->getAs<ChangeConfig>().delay, msg->getAs<ChangeConfig>().rollback ) );
                else if ( delayChanged )
                    ui.display ( format ( "Input delay was changed to %u", msg->getAs<ChangeConfig>().delay ) );
                else if ( rollbackChanged )
                    ui.display ( format ( "Rollback was changed to %u", msg->getAs<ChangeConfig>().rollback ) );
                return;

            default:
                LOG ( "Unexpected ipcReadEvent ( '%s' )", msg );
                return;
        }
    }

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
        if ( timer == stopTimer.get() )
        {
            lastError = "Timed out!";
            stop();
        }
        else if ( timer == startTimer.get() )
        {
            startTimer.reset();

            if ( !clientMode.isSpectate() )
            {
                // We must disconnect the sockets before the game process is created,
                // otherwise Windows say conflicting ports EVEN if they are created later.
                dataSocket.reset();
                serverDataSocket.reset();
                ctrlSocket.reset();
                serverCtrlSocket.reset();
            }

            // Open the game and wait for callback to ipcConnectEvent
            procMan.openGame ( appDir, ui.getConfig().getInteger ( "highCpuPriority" ) );
        }
        else
        {
            SpectatorManager::timerExpired ( timer );
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
        originalAddress = address = addr;

        if ( !appDir.empty() )
            options.set ( Options::AppDir, 1, appDir );

        if ( !ProcessManager::getIsWindowed() )
        {
            ProcessManager::setIsWindowed ( true );
            options.set ( Options::Fullscreen, 1 );
        }

        if ( options[Options::Tourney] )
        {
            clientMode.value = ClientMode::Offline;
            clientMode.flags = 0;
            options.set ( Options::Training, 0 );
            options.set ( Options::Broadcast, 0 );
            options.set ( Options::Spectate, 0 );
            options.set ( Options::Offline, 1 );
            options.set ( Options::NoUi, 1 );
        }

#ifndef RELEASE
        if ( !options[Options::StrictVersion] )
            options.set ( Options::StrictVersion, 3 );
#endif

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

        if ( ProcessManager::isWine() )
        {
            clientMode.flags |= ClientMode::IsWine;
            initialConfig.mode.flags |= ClientMode::IsWine;
            netplayConfig.mode.flags |= ClientMode::IsWine;
        }
    }

    // Destructor
    ~MainApp()
    {
        this->join();

        KeyboardManager::get().unhook();

        procMan.closeGame();

        if ( !lastError.empty() )
        {
            LOG ( "lastError='%s'", lastError );
            ui.sessionError = lastError;
        }

        syncLog.deinitialize();

        externaIpAddress.owner = 0;
    }

private:

    // Update the UI status message
    void updateStatusMessage() const
    {
        if ( isWaitingForUser )
            return;

        if ( clientMode.isBroadcast() && !isBroadcastPortReady )
            return;

        const uint16_t port = ( clientMode.isBroadcast() ? netplayConfig.broadcastPort : address.port );

        if ( externaIpAddress.address.empty() || externaIpAddress.address == ExternalIpAddress::Unknown )
        {
            ui.display ( format ( "%s on port %u%s\n",
                                  ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                  port,
                                  ( clientMode.isTraining() ? " (training mode)" : "" ) )
                         + ( externaIpAddress.address.empty()
                             ? "(Fetching external IP address...)"
                             : "(Could not find external IP address!)" ) );
        }
        else
        {
            setClipboard ( format ( "%s:%u", externaIpAddress.address, port ) );

            ui.display ( format ( "%s at %s:%u%s\n(Address copied to clipboard)",
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

        pinger.reset();
        pingStats.clear();

        uiSendSocket.reset();
        uiRecvSocket.reset();

        isBroadcastPortReady = isFinalConfigReady = isWaitingForUser = userConfirmed = false;
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
