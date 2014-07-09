#pragma once

#include "Thread.h"
#include "Util.h"
#include "IpAddrPort.h"
#include "BlockingQueue.h"

#include <memory>
#include <unordered_set>
#include <unordered_map>

class Socket;
class Timer;

class EventManager
{
    // Thread to join zombie thread
    struct ReaperThread : public Thread
    {
        // Finished threads to kill
        BlockingQueue<std::shared_ptr<Thread>> zombieThreads;

        // Thread functions
        void run() override;
        void join() override;
    };

    // Single instance of the reaper thread
    ReaperThread reaperThread;

    // Sets of active and allocated timer instances
    std::unordered_set<Timer *> activeTimers, allocatedTimers;

    // Sets of active and allocated socket instances
    std::unordered_set<Socket *> activeSockets, allocatedSockets;

    // Indicates if the hi-res timer should be used
    bool useHiResTimer;

    // Hi-res timer variables
    uint64_t ticksPerSecond, ticks;

    // The current time in milliseconds
    uint64_t now;

    // Flag to indicate the event loop is running
    volatile bool running;

    // Flag to indicate the event manager is initialized
    bool initialized;

    // Check and expire timers
    void checkTimers();

    // Check and update sockets
    void checkSockets();

    // Main event loop
    void eventLoop();

    // Private constructor, etc...
    EventManager();
    ~EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add a timer instance
    void addTimer ( Timer *timer );

    // Remove a timer instance
    void removeTimer ( Timer *timer );

    // Add a socket instance
    void addSocket ( Socket *socket );

    // Remove a socket instance
    void removeSocket ( Socket *socket );

    // Add a thread to be joined on the reaper thread
    void addThread ( const std::shared_ptr<Thread>& thread );

    // Start the event manager, blocks until stop is called
    void start();

    // Stop the event manager
    void stop();

    // Stop the event manager and release background threads
    void release();

    // Poll for events instead of start / stop, returns false on exit
    bool poll();

    // Initialize / deinitialize the event manager, should be called in the same thread as start / poll
    void initialize();
    void initializePolling();
    void deinitialize();

    // Get the current time in milliseconds
    inline uint64_t getNow() const { return now; }

    // Get the singleton instance
    static EventManager& get();
};
