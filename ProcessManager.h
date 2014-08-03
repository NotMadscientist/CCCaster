#pragma once

#include "Socket.h"
#include "Timer.h"


struct IpcConnected : public SerializableMessage
{
    EMPTY_MESSAGE_BOILERPLATE ( IpcConnected )
};


class ProcessManager : public Socket::Owner, public Timer::Owner
{
public:

    struct Owner
    {
        // IPC connected event
        inline virtual void ipcConnectEvent() {}

        // IPC disconnected event
        inline virtual void ipcDisconnectEvent() {}

        // IPC read event
        inline virtual void ipcReadEvent ( const MsgPtr& msg ) {}
    };

    Owner *owner;

private:

    // Named pipe
    void *pipe;

    // Process ID
    int processId;

    // IPC socket
    SocketPtr ipcSocket;

    // Game start timer
    TimerPtr gameStartTimer;

    // Number of attempts to start the game
    int gameStartCount;

    // IPC connect timer
    TimerPtr ipcConnectTimer;

    // IPC connected flag
    bool connected;

    // IPC socket callbacks
    void acceptEvent ( Socket *socket ) override;
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

    // IPC connect timer callback
    void timerExpired ( Timer *timer ) override;

public:

    // Basic constructor / destructor
    ProcessManager ( Owner *owner );
    ~ProcessManager();

    // Open / close the game from the EXE side
    void openGame();
    void closeGame();

    // Connect / disconnect the IPC pipe and socket from the DLL side
    void connectPipe();
    void disconnectPipe();

    // Indicates if the IPC pipe and socket are connected
    inline bool ipcConnected() const { return ( pipe && ipcSocket && ipcSocket->isClient() && connected ); }

    // Send a message over the IPC socket
    inline bool ipcSend ( Serializable *msg ) { return ipcSend ( MsgPtr ( msg ) ); }
    inline bool ipcSend ( const MsgPtr& msg ) { if ( !ipcSocket ) return false; else return ipcSocket->send ( msg ); }

    // Get the process ID of the game
    inline int getProcessId() const { return processId; }

    // Write game inputs
    void writeGameInputs ( uint8_t player, uint16_t direction, uint16_t buttons );
};
