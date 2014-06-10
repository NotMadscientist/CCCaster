#pragma once

#include "Thread.h"
#include "BlockingQueue.h"
#include "IpAddrPort.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <string>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <cstdio>

#define READ_BUFFER_SIZE ( 1024 * 4096 )

#define NL_SOCKET_GROUP_CMD(NAME)                                   \
    class NAME : public NL::SocketGroupCmd {                        \
        Socket& context;                                            \
    public:                                                         \
        NAME ( Socket& context ) : context ( context ) {}           \
        void exec ( NL::Socket *, NL::SocketGroup *, void * );      \
    }

class Socket
{
    class ListenThread : public Thread
    {
        Socket& context;
        mutable Mutex mutex;
        bool isListening;

    public:

        ListenThread ( Socket& context )
            : context ( context ), isListening ( false ) {}

        void start();
        void join();
        void run();
    };

    class TcpConnectThread : public Thread
    {
        Socket& context;
        const IpAddrPort address;

    public:

        TcpConnectThread ( Socket& context, const IpAddrPort& address )
            : context ( context ), address ( address ) {}

        void run();
    };

    char readBuffer[READ_BUFFER_SIZE];

    struct ReaperThread : public Thread { void run(); void join(); };

    std::shared_ptr<NL::Socket> serverSocket;
    std::shared_ptr<NL::Socket> tcpSocket, udpSocket;
    std::shared_ptr<NL::SocketGroup> socketGroup;
    std::unordered_map<IpAddrPort, std::shared_ptr<NL::Socket>> acceptedSockets;

    NL_SOCKET_GROUP_CMD ( Accept ) socketAcceptCmd;
    NL_SOCKET_GROUP_CMD ( Disconnect ) socketDisconnectCmd;
    NL_SOCKET_GROUP_CMD ( Read ) socketReadCmd;

    ListenThread listenThread;

    static BlockingQueue<std::shared_ptr<Thread>> connectingThreads;

    static ReaperThread reaperThread;

protected:

    mutable Mutex mutex;

    void addConnectingThread ( const std::shared_ptr<Thread>& thread );

    void addSocketToGroup ( const std::shared_ptr<NL::Socket>& socket );

public:

    struct Owner
    {
        virtual Owner *accepted ( Socket *socket ) { return 0; }
        virtual void connected ( Socket *socket ) {}
        virtual void disconnected ( Socket *socket ) {}
        virtual void received ( Socket *socket, char *bytes, std::size_t len, const IpAddrPort& address ) {}
    };

    Socket();
    virtual ~Socket();

    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    void listen ( unsigned port );
    void tcpConnect ( const IpAddrPort& address );
    void udpConnect ( const IpAddrPort& address );
    void tcpDisconnect ( const IpAddrPort& address = IpAddrPort() );
    void udpDisconnect();

    bool isServer() const;
    bool isConnected() const;

    void tcpSend ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void tcpSend ( char *bytes, std::size_t len, const IpAddrPort& address = IpAddrPort() );
    void udpSend ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void udpSend ( char *bytes, std::size_t len, const IpAddrPort& address = IpAddrPort() );

    static void release();
};
