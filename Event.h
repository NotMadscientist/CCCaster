#pragma once

#include "Thread.h"
#include "Util.h"
#include "BlockingQueue.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <memory>
#include <array>
#include <unordered_map>

#define NL_SOCKET_GROUP_CMD(NAME)                                                                       \
    class NAME : public NL::SocketGroupCmd {                                                            \
        std::unordered_map<NL::Socket *, Socket *>& socketMap;                                          \
    public:                                                                                             \
        NAME ( std::unordered_map<NL::Socket *, Socket *>& socketMap ) : socketMap ( socketMap ) {}     \
        void exec ( NL::Socket *, NL::SocketGroup *, void * );                                          \
    }

class Socket;

class Timer;

class EventManager
{
    mutable Mutex mutex;

    bool running;

    // Thread stuff

    class ReaperThread : public Thread
    {
        BlockingQueue<std::shared_ptr<Thread>>& zombieThreads;

    public:

        ReaperThread ( BlockingQueue<std::shared_ptr<Thread>>& zombieThreads ) : zombieThreads ( zombieThreads ) {}

        void run();
        void join();
    };

    BlockingQueue<std::shared_ptr<Thread>> zombieThreads;

    ReaperThread reaperThread;

    // Socket stuff

    class TcpConnectThread : public Thread
    {
        Socket *socket;

    public:

        TcpConnectThread ( Socket *socket ) : socket ( socket ) {}

        void run();
    };

    mutable CondVar socketsCond;

    NL::SocketGroup socketGroup;

    std::unordered_map<NL::Socket *, Socket *> socketMap;
    std::unordered_map<NL::Socket *, std::shared_ptr<NL::Socket>> rawSocketMap;

    std::vector<NL::Socket *> rawSocketsToAdd, rawSocketsToRemove;

    NL_SOCKET_GROUP_CMD ( SocketAccept ) socketAcceptCmd;
    NL_SOCKET_GROUP_CMD ( SocketDisconnect ) socketDisconnectCmd;
    NL_SOCKET_GROUP_CMD ( SocketRead ) socketReadCmd;

    void socketListenLoop();

    // Timer stuff

    class TimerThread : public Thread
    {
        EventManager& context;
        volatile bool running;
        bool useHiRes;

        void checkTimers ( double now );

    public:

        TimerThread ( EventManager& context, bool useHiRes = true )
            : context ( context ), running ( false ), useHiRes ( useHiRes ) {}

        void start();
        void join();
        void run();
    };

    mutable CondVar timersCond;

    TimerThread timerThread;

    std::unordered_set<Timer *> timerSet;

    // General stuff

    EventManager();

    ~EventManager();

    EventManager ( const EventManager& );

    const EventManager& operator= ( const EventManager& );

public:

    void addThread ( const std::shared_ptr<Thread>& thread );

    void addSocket ( Socket *socket );

    void removeSocket ( Socket *socket );

    void addTimer ( Timer *timer );

    void removeTimer ( Timer *timer );

    double now() const;

    void start();

    void stop();

    static EventManager& get();
};

#undef NL_SOCKET_GROUP_CMD
