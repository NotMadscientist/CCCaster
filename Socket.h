#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

#include <iostream>

enum class Protocol : uint8_t { TCP, UDP };

struct Socket
{
    struct Owner
    {
        // Accepted a TCP socket from server socket
        virtual void acceptEvent ( Socket *serverSocket ) {}

        // TCP connected event
        virtual void connectEvent ( Socket *socket ) {}

        // TCP disconnected event
        virtual void disconnectEvent ( Socket *socket ) {}

        // Read event
        virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) {}
    };

    Owner *owner;

private:

    // Underlying raw socket
    std::shared_ptr<NL::Socket> socket;

    // Currently accepted TCP socket
    std::shared_ptr<Socket> acceptedSocket;

    const IpAddrPort address;
    const Protocol protocol;

    std::string readBuffer;
    size_t readPos;

    Socket ( NL::Socket *socket );
    Socket ( Owner *owner, unsigned port, Protocol protocol );
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

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, const Protocol& protocol );
std::ostream& operator<< ( std::ostream& os, const NL::Protocol& protocol );
