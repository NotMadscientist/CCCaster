#pragma once

#include "Thread.h"
#include "Util.h"
#include "BlockingQueue.h"
#include "IpAddrPort.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <memory>
#include <array>
#include <unordered_map>

#define SOCKET_READ_BUFFER_SIZE ( 1024 * 4096 )

#define NL_SOCKET_GROUP_CMD(NAME)                                                                       \
    class NAME : public NL::SocketGroupCmd {                                                            \
        std::unordered_map<NL::Socket *, Socket *>& socketMap;                                          \
    public:                                                                                             \
        NAME ( std::unordered_map<NL::Socket *, Socket *>& socketMap ) : socketMap ( socketMap ) {}     \
        void exec ( NL::Socket *, NL::SocketGroup *, void * );                                          \
    }

class Socket;

class EventManager
{
    class ReaperThread : public Thread
    {
        BlockingQueue<std::shared_ptr<Thread>>& zombieThreads;

    public:

        ReaperThread ( BlockingQueue<std::shared_ptr<Thread>>& zombieThreads )
            : zombieThreads ( zombieThreads ) {}

        void run();
        void join();
    };

    class TcpConnectThread : public Thread
    {
        Socket *socket;
        const IpAddrPort address;

    public:

        TcpConnectThread ( Socket *socket, const IpAddrPort& address )
            : socket ( socket ), address ( address ) {}

        void run();
    };

    mutable Mutex mutex;

    NL::SocketGroup socketGroup;

    std::unordered_map<NL::Socket *, Socket *> socketMap;

    NL_SOCKET_GROUP_CMD ( SocketAccept ) socketAcceptCmd;
    NL_SOCKET_GROUP_CMD ( SocketDisconnect ) socketDisconnectCmd;
    NL_SOCKET_GROUP_CMD ( SocketRead ) socketReadCmd;

    BlockingQueue<std::shared_ptr<Thread>> zombieThreads;

    ReaperThread reaperThread;

    EventManager();

    ~EventManager();

    EventManager ( const EventManager& );

    const EventManager& operator= ( const EventManager& );

public:

    void addThread ( const std::shared_ptr<Thread>& thread );

    void addSocket ( Socket *socket );

    void removeSocket ( Socket *socket );

    void connectTcpSocket ( Socket *socket, const IpAddrPort& address );

    void start();

    static EventManager& get();
};
