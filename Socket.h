#pragma once

// #include <winsock2.h>
// #include <windows.h>

#include "Thread.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <string>
#include <memory>

#define NL_SOCKET_GROUP_CMD(NAME)                                   \
    struct NAME : public NL::SocketGroupCmd {                       \
        Socket& context;                                            \
        NAME ( Socket& context ) : context ( context ) {}           \
        void exec ( NL::Socket *, NL::SocketGroup *, void * );      \
    }

class Socket
{
    std::shared_ptr<NL::Socket> serverSocket;
    std::shared_ptr<NL::Socket> tcpSocket, udpSocket;
    std::shared_ptr<NL::SocketGroup> socketGroup;

    NL_SOCKET_GROUP_CMD ( Accept ) socketAccept;
    NL_SOCKET_GROUP_CMD ( Disconnect ) socketDisconnect;
    NL_SOCKET_GROUP_CMD ( Read ) socketRead;

    THREAD ( ListenThread, Socket ) listenThread;

protected:
    mutable Mutex mutex;

    virtual void accepted ( uint32_t id ) {};

    virtual void connected() {};

    virtual void received ( uint32_t id, char *bytes, std::size_t len ) {};

public:
    Socket()
        : socketAccept ( *this )
        , socketDisconnect ( *this )
        , socketRead ( *this )
        , listenThread ( *this )
    {
    }

    virtual ~Socket() {}

    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    void listen ( int port );

    void connect ( std::string addr, int port );

    void relay ( std::string room, std::string server, int port );

    void disconnect();

    bool isConnected() const;
    std::string localAddr() const;
    std::string remoteAddr ( uint32_t id = 0 ) const;

    void send ( uint32_t id, char *bytes, std::size_t len );
};
