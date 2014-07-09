#pragma once

#include "Socket.h"

class TcpSocket : public Socket
{
    // Construct a server socket
    TcpSocket ( Socket::Owner *owner, uint16_t port );

    // Construct a client socket
    TcpSocket ( Socket::Owner *owner, const IpAddrPort& address );

    // Construct an accepted client socket
    TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address );

protected:

    // Socket event callbacks
    virtual void acceptEvent() override;
    virtual void connectEvent() override;
    virtual void disconnectEvent() override;
    virtual void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Listen for connections on the given port
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address );

    // Virtual destructor
    ~TcpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // Send a protocol message, return false indicates disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;
};
