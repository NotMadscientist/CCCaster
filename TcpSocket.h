#pragma once

#include "Socket.h"

class TcpSocket : public Socket
{
    // Construct a server socket
    TcpSocket ( Owner *owner, unsigned port );

    // Construct a client socket
    TcpSocket ( Owner *owner, const IpAddrPort& address );

    // Construct an accepted client socket
    TcpSocket ( Owner *owner, int fd, const IpAddrPort& address );

protected:

    // EventManager callbacks
    virtual void acceptEvent() override;
    virtual void connectEvent() override;
    virtual void disconnectEvent() override;
    virtual void readEvent() override;

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Owner *owner, unsigned port );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect ( Owner *owner, const IpAddrPort& address );

    // Virtual destructor
    ~TcpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    std::shared_ptr<Socket> accept ( Owner *owner ) override;

    // Send a protocol message, return false indicates disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;
};
