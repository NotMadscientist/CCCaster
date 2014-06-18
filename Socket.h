#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

#include <iostream>

enum class Protocol : uint8_t { TCP, UDP };

struct Socket
{
    struct Owner
    {
        virtual void acceptEvent ( Socket *serverSocket ) {}
        virtual void connectEvent ( Socket *socket ) {}
        virtual void disconnectEvent ( Socket *socket ) {}
        virtual void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address ) {}
    };

private:

    Owner *owner;
    NL::Socket *socket;

    std::shared_ptr<Socket> acceptedSocket;

    const IpAddrPort address;
    const Protocol protocol;

    Socket ( NL::Socket *socket );
    Socket ( Owner *owner, unsigned port, Protocol protocol );
    Socket ( Owner *owner, const std::string& address, unsigned port, Protocol protocol );

public:

    static std::shared_ptr<Socket> listen ( Owner *owner, unsigned port, Protocol protocol );
    static std::shared_ptr<Socket> connect (
        Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    ~Socket();

    void disconnect();

    std::shared_ptr<Socket> accept ( Owner *owner );

    inline bool isConnected() const { return ( socket != 0 ); }
    inline bool isServer() const { if ( !isConnected() ) return false; return ( socket->type() == NL::SERVER ); }

    inline const IpAddrPort& getRemoteAddress() const { return address; }
    inline Protocol getProtocol() const { return protocol; }

    void send ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    friend class EventManager;
    friend class DoubleSocket;
};

std::ostream& operator<< ( std::ostream& os, const Protocol& protocol );
std::ostream& operator<< ( std::ostream& os, const NL::Protocol& protocol );
