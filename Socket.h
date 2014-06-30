#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

#include <iostream>

enum class Protocol : uint8_t { TCP, UDP };

class Socket
{
public:

    // Socket owner interface
    struct Owner
    {
        // Accepted a socket from server socket
        inline virtual void acceptEvent ( Socket *serverSocket ) { serverSocket->accept ( this )->disconnect(); }

        // Socket connected event
        inline virtual void connectEvent ( Socket *socket ) {}

        // Socket disconnected event
        inline virtual void disconnectEvent ( Socket *socket ) {}

        // Socket read event
        inline virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) {}
    };

    // Set the socket owner
    inline virtual void setOwner ( Owner *owner ) { this->owner = owner; }

private:

    // Underlying raw socket
    std::shared_ptr<NL::Socket> socket;

    // Socket protocol
    const Protocol protocol;

    // Socket read buffer and position
    std::string readBuffer;
    size_t readPos;

    // Simulated packet loss percentage for testing
    uint8_t packetLoss;

    // Construct with an existing raw socket
    Socket ( NL::Socket *socket );

protected:

    // Socket owner
    Owner *owner;

    // Socket address
    const IpAddrPort address;

    // Currently accepted socket
    std::shared_ptr<Socket> acceptedSocket;

    // Construct a server socket
    Socket ( Owner *owner, unsigned port, Protocol protocol );

    // Construct a client socket
    Socket ( Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    // Construct a proxy socket (for UDP server clients)
    Socket ( Owner *owner, const std::string& address, unsigned port );

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Owner *owner, unsigned port, Protocol protocol );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect (
        Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    // Virtual destructor
    virtual ~Socket();

    // Completely disconnect the socket
    virtual void disconnect();

    // Accept a new socket
    virtual std::shared_ptr<Socket> accept ( Owner *owner );

    // Socket status
    inline virtual bool isConnected() const { return ( isClient() && socket.get() ); }
    inline virtual bool isClient() const { return !address.addr.empty(); }
    inline virtual bool isServer() const { return address.addr.empty(); }

    // Socket parameters
    inline virtual const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }
    inline virtual Protocol getProtocol() const { return protocol; }

    // Send a protocol message
    virtual void send ( Serializable *message, const IpAddrPort& address = IpAddrPort() );
    virtual void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() );

    // Send raw bytes directly
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    // Set the packet loss for testing purposes
    void setPacketLoss ( uint8_t percentage );

    // Get the number of sockets pending acceptance for testing purposes
    inline virtual size_t getPendingCount() const { return ( acceptedSocket.get() ? 1 : 0 ); };

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, const Protocol& protocol );
std::ostream& operator<< ( std::ostream& os, const NL::Protocol& protocol );
