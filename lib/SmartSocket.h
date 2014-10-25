#pragma once

#include "Socket.h"
#include "Timer.h"


class SmartSocket : public Socket, public Socket::Owner, public Timer::Owner
{
    // The backing socket, ie the first socket that tries to connect directly
    SocketPtr backingSocket;

    // The socket that connects to the notification and tunnel server
    SocketPtr vpsSocket;

    // The UDP tunnel socket
    SocketPtr tunSocket;

    // The initial connect timer
    TimerPtr connectTimer;

    // The UDP tunnel send timer
    TimerPtr sendTimer;

    // Unused base socket callback
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override {}

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override;
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
    void readEvent ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address ) override;

    // Timer callback
    void timerExpired ( Timer *timer ) override;

    // Construct a server socket
    SmartSocket ( Socket::Owner *owner, Socket::Protocol protocol, uint16_t port );

    // Construct a client socket
    SmartSocket ( Socket::Owner *owner, Socket::Protocol protocol, const IpAddrPort& address );

public:

    // Listen for connections on the given port.
    // Opens a regular server socket of the given protocol, but also listens for UDP tunnel connections.
    static SocketPtr listen ( Socket::Owner *owner, Socket::Protocol protocol, uint16_t port );

    // Connect to the given address and port.
    // Tries to connect using the given protocol, with fallback to UDP tunnel.
    static SocketPtr connect ( Socket::Owner *owner, Socket::Protocol protocol, const IpAddrPort& address );

    // Destructor
    ~SmartSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // Send raw bytes directly, a return value of false indicates socket is disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = NullAddress ) override;
};
