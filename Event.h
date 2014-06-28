#pragma once

#include "Thread.h"
#include "Util.h"
#include "IpAddrPort.h"
#include "BlockingQueue.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <memory>
#include <unordered_set>
#include <unordered_map>

#define NL_GROUP_CMD(NAME, VAR) \
    struct NAME : public NL::SocketGroupCmd { void exec ( NL::Socket *, NL::SocketGroup *, void * ) override; } VAR

class Socket;
class Timer;

class EventManager
{
    // Main event mutex
    mutable Mutex mutex;

    // Flag to indicate the event loop(s) are running
    bool running;

    // Thread stuff

    // Thread to join zombie thread
    class ReaperThread : public Thread
    {
        BlockingQueue<std::shared_ptr<Thread>>& zombieThreads;

    public:

        ReaperThread ( BlockingQueue<std::shared_ptr<Thread>>& zombieThreads ) : zombieThreads ( zombieThreads ) {}

        void run() override;
        void join() override;
    };

    // Finished threads to kill
    BlockingQueue<std::shared_ptr<Thread>> zombieThreads;

    // Single instance of the reaper thread
    ReaperThread reaperThread;

    // Socket stuff

    // Thread to connect TCP sockets
    class TcpConnectThread : public Thread
    {
        Socket *socket;
        IpAddrPort address;

    public:

        TcpConnectThread ( Socket *socket, const IpAddrPort& address ) : socket ( socket ), address ( address ) {}

        void run() override;
    };

    // Condition var to signal changes to the list of sockets
    mutable CondVar socketsCond;

    // Raw sockets to listen on
    NL::SocketGroup socketGroup;

    // Map of raw socket to socket instance, for socket events
    std::unordered_map<NL::Socket *, Socket *> rawSocketToSocket;

    // Set of active and connecting socket instances
    std::unordered_set<Socket *> activeSockets, connectingSockets;

    // Map of socket instance to corresponding raw socket, for ownership
    std::unordered_map<Socket *, std::shared_ptr<NL::Socket>> activeRawSockets;

    // Socket read events
    NL_GROUP_CMD ( SocketAccept,     socketAcceptCmd );
    NL_GROUP_CMD ( SocketDisconnect, socketDisconnectCmd );
    NL_GROUP_CMD ( SocketRead,       socketReadCmd );

    // Main socket event loop
    void socketListenLoop();

    // Timer stuff

    // Thread to run the hi-resolution timer
    class TimerThread : public Thread
    {
        volatile bool running;
        bool useHiRes;

        void checkTimers();

    public:

        TimerThread ( bool useHiRes = true ) : running ( false ), useHiRes ( useHiRes ) {}

        void start() override;
        void join() override;
        void run() override;
    };

    // Condition var to signal changes to the list of timers
    mutable CondVar timersCond;

    // Single instance of the timer thread
    TimerThread timerThread;

    // Set of active timer instances
    std::unordered_set<Timer *> activeTimers;

    // General stuff

    // Private constructor, etc...
    EventManager();
    ~EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add a thread to be joined on the reaper thread
    void addThread ( const std::shared_ptr<Thread>& thread );

    // Add a socket instance
    void addSocket ( Socket *socket );

    // Remove a socket instance
    void removeSocket ( Socket *socket );

    // Add a timer instance
    void addTimer ( Timer *timer );

    // Remove a timer instance
    void removeTimer ( Timer *timer );

    // Start the event manager
    void start();

    // Stop the event manager
    void stop();

    // Stop the event manager and release background threads
    void release();

    // Indicates if the event manager is running
    inline bool isRunning() const
    {
        LOCK ( mutex );
        return running;
    }

    // Get the singleton instance
    static EventManager& get();
};
