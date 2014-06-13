#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

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

    Owner& owner;
    NL::Socket *socket;

    const IpAddrPort address;
    const Protocol protocol;

    Socket ( Owner& owner, NL::Socket *socket );
    Socket ( Owner& owner, unsigned port, Protocol protocol );
    Socket ( Owner& owner, const std::string& address, unsigned port, Protocol protocol );

public:

    static Socket *listen ( Owner& owner, unsigned port, Protocol protocol );
    static Socket *connect ( Owner& owner, const std::string& address, unsigned port, Protocol protocol );

    ~Socket();

    void disconnect();

    Socket *accept ( Owner& owner );

    inline bool isConnected() const { return ( socket != 0 ); }
    inline bool isServer() const { if ( !isConnected() ) return false; return ( socket->type() == NL::SERVER ); }

    inline const IpAddrPort& getRemoteAddress() const { return address; }
    inline Protocol getProtocol() const { return protocol; }

    void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    friend class EventManager;
};
