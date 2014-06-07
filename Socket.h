#pragma once

#include "Thread.h"
#include "BlockingQueue.h"

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

struct IpAddrPort
{
    std::string addr;
    unsigned port;

    IpAddrPort() : port ( 0 ) {}

    IpAddrPort ( const std::shared_ptr<NL::Socket>& socket )
        : addr ( socket->hostTo() ), port ( socket->portTo() ) {}

    IpAddrPort ( const NL::Socket *socket )
        : addr ( socket->hostTo() ), port ( socket->portTo() ) {}

    IpAddrPort ( const std::string& addr, unsigned port )
        : addr ( addr ), port ( port ) {}

    inline bool empty() const
    {
        return ( addr.empty() || !port );
    }

    inline void clear()
    {
        addr.clear();
        port = 0;
    }

    inline std::string getAddrPort() const
    {
        if ( empty() )
            return "";
        std::stringstream ss;
        ss << addr << ':' << port;
        return ss.str();
    }

    inline const char *c_str() const
    {
        if ( empty() )
            return "";
        static char buffer[256];
        std::sprintf ( buffer, "%s:%u", addr.c_str(), port );
        return buffer;
    }
};

inline bool operator== ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ( a.addr == b.addr && a.port == b.port );
}

inline bool operator!= ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ! ( a == b );
}

namespace std
{

template<class T>
inline void hash_combine ( size_t& seed, const T& v )
{
    hash<T> hasher;
    seed ^= hasher ( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

template<> struct hash<IpAddrPort>
{
    inline size_t operator() ( const IpAddrPort& a ) const
    {
        size_t seed = 0;
        hash_combine ( seed, a.addr );
        hash_combine ( seed, a.port );
        return seed;
    }
};

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
    std::unordered_map<IpAddrPort, std::shared_ptr<NL::Socket>> acceptedSockets;

    NL_SOCKET_GROUP_CMD ( Accept ) socketAcceptCmd;
    NL_SOCKET_GROUP_CMD ( Disconnect ) socketDisconnectCmd;
    NL_SOCKET_GROUP_CMD ( Read ) socketReadCmd;

    ListenThread listenThread;

    static BlockingQueue<std::shared_ptr<ConnectThread>> connectingThreads;

    static ReaperThread reaperThread;

protected:

    mutable Mutex mutex;

    void addSocketToGroup ( const std::shared_ptr<NL::Socket>& socket );

    virtual void tcpAccepted ( const IpAddrPort& address ) {}
    virtual void tcpConnected ( const IpAddrPort& address ) {}
    virtual void tcpDisconnected ( const IpAddrPort& address ) {}

    virtual void tcpReceived ( char *bytes, std::size_t len, const IpAddrPort& address ) {}
    virtual void udpReceived ( char *bytes, std::size_t len, const IpAddrPort& address ) {}

public:

    Socket();
    virtual ~Socket();

    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }

    void listen ( unsigned port );
    void tcpConnect ( const std::string& addr, unsigned port );
    void udpConnect ( const std::string& addr, unsigned port );
    void tcpDisconnect ( const IpAddrPort& address = IpAddrPort() );
    void udpDisconnect();

    bool isServer() const;
    bool isConnected() const;

    void tcpSend ( char *bytes, std::size_t len, const IpAddrPort& address = IpAddrPort() );
    void udpSend ( char *bytes, std::size_t len, const IpAddrPort& address = IpAddrPort() );

    static void release();
};
