#pragma once

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <string>
#include <memory>

class Socket
{
    struct TcpA : NL::SocketGroupCmd { void exec ( NL::Socket *, NL::SocketGroup *, void * ); } tcpAccept;
    struct TcpD : NL::SocketGroupCmd { void exec ( NL::Socket *, NL::SocketGroup *, void * ); } tcpDisconnect;
    struct TcpR : NL::SocketGroupCmd { void exec ( NL::Socket *, NL::SocketGroup *, void * ); } tcpRead;
    struct UdpR : NL::SocketGroupCmd { void exec ( NL::Socket *, NL::SocketGroup *, void * ); } udpRead;

    std::shared_ptr<NL::Socket> serverSocket;
    std::shared_ptr<NL::Socket> tcpSocket, udpSocket;
    std::shared_ptr<NL::SocketGroup> socketGroup;

protected:
    virtual void accept ( std::shared_ptr<Socket>& socket ) {};

    virtual void recv ( char *bytes, std::size_t len ) {};

public:
    Socket() {}
    virtual ~Socket() {}

    void relay ( std::string room, std::string server, int port );

    void connect ( std::string addr, int port );

    void listen ( int port );

    void disconnect();

    void send ( char *bytes, std::size_t len );
};
