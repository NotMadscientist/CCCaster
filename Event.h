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

    // The current time in milliseconds
    uint64_t now;

    // Flag to indicate the event loop is running
    bool running;

    // Flag to indicate the event manager is initialized
    static bool initialized;

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

    // Start the event manager
    void start();

    // Stop the event manager
    void stop();

    // Stop the event manager and release background threads
    void release();

    // Indicates if the event manager is running
    inline bool isRunning() const { return running; }

    // Get the current time in milliseconds
    inline const uint64_t& getNow() const { return now; }

    // Indicates if the event manager is initialized
    inline static bool isInitialized() { return initialized; }

    // Initialize the event manager
    static void initialize();

    // Get the singleton instance
    static EventManager& get();
};
