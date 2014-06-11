#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

struct Socket
{
    struct Owner
    {
        virtual void acceptEvent ( Socket *serverSocket ) {}
        virtual void connectEvent ( Socket *socket ) {}
        virtual void disconnectEvent ( Socket *socket ) {}
        virtual void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address ) {}
    };

    enum Protocol { TCP, UDP };

private:

    Owner& owner;
    NL::Socket *socket;

    Socket ( Owner& owner, NL::Socket *socket );
    Socket ( Owner& owner, unsigned port, Protocol protocol );
    Socket ( Owner& owner, const std::string& address, unsigned port, Protocol protocol );

protected:

    const IpAddrPort address;
    const Protocol protocol;

public:

    static Socket *listen ( Owner& owner, unsigned port, Protocol protocol );
    static Socket *connect ( Owner& owner, const std::string& address, unsigned port, Protocol protocol );

    ~Socket();

    void disconnect();

    Socket *accept ( Owner& owner );

    bool isServer() const;
    bool isConnected() const;

    IpAddrPort getRemoteAddress() const;

    void send ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    friend class EventManager;
};
