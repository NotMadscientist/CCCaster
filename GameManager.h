#pragma once

#include "Socket.h"
#include "Timer.h"


class GameManager : public Socket::Owner, public Timer::Owner
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

    // IPC connect timer
    TimerPtr ipcConnectTimer;

    // IPC socket callbacks
    void acceptEvent ( Socket *socket ) override;
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

    // IPC connect timer callback
    void timerExpired ( Timer *timer ) override;

public:

    // Basic constructor / destructor
    inline GameManager ( Owner *owner ) : owner ( owner ), pipe ( 0 ), processId ( 0 ) {}
    ~GameManager();

    // Open / close the game from the EXE side
    void openGame();
    void closeGame();

    // Connect / disconnect the IPC pipe and socket from the DLL side
    void connectPipe();
    void disconnectPipe();

    // Indicates if the IPC pipe and socket are connected
    inline bool isConnected() const { return ( pipe && ipcSocket && ipcSocket->isClient() ); }
};
