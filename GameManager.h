#pragma once

#include "Socket.h"


class GameManager : public Socket::Owner
{
public:

    struct Owner
    {
        inline virtual void gameOpened() {}
        inline virtual void gameClosed() {}
    };

    Owner *owner;

private:

    // Named pipe
    void *pipe;

    // Process ID
    int processId;

    // IPC socket
    SocketPtr ipcSocket;

    // IPC socket callbacks
    void connectEvent ( Socket *socket );
    void disconnectEvent ( Socket *socket );
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address );

public:

    // Basic constructor
    inline GameManager ( Owner *owner ) : owner ( owner ), pipe ( 0 ), processId ( 0 ) {}

    // Open / close the game
    void openGame();
    void closeGame();
};
