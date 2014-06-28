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
        inline virtual void acceptEvent ( Socket *serverSocket ) {}

        // Socket connected event
        inline virtual void connectEvent ( Socket *socket ) {}

        // Socket disconnected event
        inline virtual void disconnectEvent ( Socket *socket ) {}

        // Socket read event
        inline virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) {}
    };

    // Socket owner
    Owner *owner;

private:

    // Underlying raw socket
    std::shared_ptr<NL::Socket> socket;

    // Currently accepted TCP socket
    std::shared_ptr<Socket> acceptedSocket;

    // Socket address
    const IpAddrPort address;

    // Socket protocol
    const Protocol protocol;

    // Socket read buffer and position
    std::string readBuffer;
    size_t readPos;

    // Simulated packet loss percentage for testing
    uint8_t packetLoss;

    // Construct with an existing raw socket
    Socket ( NL::Socket *socket );

    // Construct a server socket
    Socket ( Owner *owner, unsigned port, Protocol protocol );

    // Construct a client socket
    Socket ( Owner *owner, const std::string& address, unsigned port, Protocol protocol );

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Owner *owner, unsigned port, Protocol protocol );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect (
        Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    ~Socket();

    // Completely disconnect the socket
    void disconnect();

    // Accept a new TCP socket
    std::shared_ptr<Socket> accept ( Owner *owner );

    inline bool isConnected() const { return ( isClient() && socket.get() ); }
    inline bool isClient() const { return !address.addr.empty(); }
    inline bool isServer() const { return address.addr.empty(); }

    inline const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }
    inline Protocol getProtocol() const { return protocol; }

    void send ( Serializable *message, const IpAddrPort& address = IpAddrPort() );
    void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    // Set the packet loss for testing purposes
    void setPacketLoss ( uint8_t percentage );

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, const Protocol& protocol );
std::ostream& operator<< ( std::ostream& os, const NL::Protocol& protocol );
