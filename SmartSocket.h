#pragma once

#include "Socket.h"

class SmartSocket : private Socket
{
    std::shared_ptr<NL::Socket> reliableUdpSocket;

    void tcpAccepted ( const std::string& addrPort );
    void tcpConnected ( const std::string& addrPort );
    void tcpDisconnected ( const std::string& addrPort );

    void tcpReceived ( char *bytes, std::size_t len, const std::string& addrPort );
    void udpReceived ( char *bytes, std::size_t len, const std::string& addr, unsigned port );

protected:

    virtual void accepted ( ADDRESS address ) {}
    virtual void connected ( ADDRESS address ) {}
    virtual void disconnected ( ADDRESS address ) {}

    virtual void receivedPrimary ( char *bytes, std::size_t len, ADDRESS address ) {}
    virtual void receivedSecondary ( char *bytes, std::size_t len, ADDRESS address ) {}

public:

    SmartSocket();
    ~SmartSocket();

    void connect ( const std::string& addr, unsigned port );
    void relay ( const std::string& room, const std::string& server, unsigned port );

    void sendPrimary ( char *bytes, std::size_t len, ADDRESS address );
    void sendSecondary ( char *bytes, std::size_t len, ADDRESS address );
};
