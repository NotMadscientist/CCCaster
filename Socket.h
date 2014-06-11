#pragma once

#include "IpAddrPort.h"

#include <netlink/socket.h>

#include <memory>

struct Socket
{
    struct Owner
    {
        virtual void acceptEvent ( Socket *serverSocket ) {}
        virtual void connectEvent ( Socket *socket ) {}
        virtual void disconnectEvent ( Socket *socket ) {}
        virtual void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address ) {}
    };

    enum Proto { TCP, UDP };

private:

    Owner& owner;
    std::shared_ptr<NL::Socket> socket;

    Socket ( Owner& owner, NL::Socket *socket );
    Socket ( Owner& owner, const std::shared_ptr<NL::Socket>& socket );
    Socket ( Owner& owner, const std::string& address, unsigned port );

public:

    ~Socket();

    static std::shared_ptr<Socket> listen ( Owner& owner, unsigned port, Proto protocol );
    static std::shared_ptr<Socket> connect ( Owner& owner, const std::string& address, unsigned port, Proto protocol );

    std::shared_ptr<Socket> accept ( Owner& owner );

    void disconnect();

    bool isServer() const;
    bool isConnected() const;

    void send ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void send ( char *bytes, size_t len, const IpAddrPort& address = IpAddrPort() );

    friend class EventManager;
};
