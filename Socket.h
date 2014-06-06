#pragma once

#include "Thread.h"
#include "BlockingQueue.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <string>
#include <memory>
#include <unordered_map>

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
        volatile bool isListening;

    public:

        ListenThread ( Socket& context )
            : context ( context ), isListening ( false ) {}
        void start();
        void join();
        void run();
    };

    class ConnectThread : public Thread
    {
        Socket& context;
        const std::string addr;
        const int port;

    public:

        ConnectThread ( Socket& context, const std::string& addr, int port )
            : context ( context ), addr ( addr ), port ( port ) {}
        void start();
        void run();
    };

    char readBuffer[READ_BUFFER_SIZE];

    struct ReaperThread : public Thread { void run(); void join(); };

    std::shared_ptr<NL::Socket> serverSocket;
    std::shared_ptr<NL::Socket> tcpSocket, udpSocket;
    std::shared_ptr<NL::SocketGroup> socketGroup;
    std::unordered_map<std::string, std::shared_ptr<NL::Socket>> acceptedSockets;

    NL_SOCKET_GROUP_CMD ( Accept ) socketAcceptCmd;
    NL_SOCKET_GROUP_CMD ( Disconnect ) socketDisconnectCmd;
    NL_SOCKET_GROUP_CMD ( Read ) socketReadCmd;

    ListenThread listenThread;

    static BlockingQueue<std::shared_ptr<ConnectThread>> connectingThreads;

    static ReaperThread reaperThread;

protected:

    mutable Mutex mutex;

    void addSocketToGroup ( const std::shared_ptr<NL::Socket>& socket );

    virtual void tcpAccepted ( const std::string& addrPort ) {}
    virtual void tcpConnected ( const std::string& addrPort ) {}
    virtual void tcpDisconnected ( const std::string& addrPort ) {}

    virtual void tcpReceived ( char *bytes, std::size_t len, const std::string& addrPort ) {}
    virtual void udpReceived ( char *bytes, std::size_t len, const std::string& addr, unsigned port ) {}

public:

    Socket();
    virtual ~Socket();

    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    void listen ( unsigned port );
    void tcpConnect ( const std::string& addr, unsigned port );
    void udpConnect ( const std::string& addr, unsigned port );
    void disconnect ( const std::string& addrPort = "" );

    bool isServer() const;
    bool isConnected() const;

    void tcpSend ( char *bytes, std::size_t len, const std::string& addrPort = "" );
    void udpSend ( char *bytes, std::size_t len, const std::string& addr = "", unsigned port = 0 );

    static void release();

    static std::string getAddrPort ( const std::shared_ptr<NL::Socket>& socket );
    static std::string getAddrPort ( const NL::Socket *socket );
    static std::string getAddrPort ( const std::string& addr, unsigned port );
};
