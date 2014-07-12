#pragma once

#include "Thread.h"
#include "BlockingQueue.h"

#include <memory>

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

    // Sets of active and allocated timer instanc
    // Flag to indicate the event loop is running
    volatile bool running;

    // Check for events
    void checkEvents();

    // Main event loop
    void eventLoop();

    // Private constructor, etc. for singleton class
    EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add a thread to be joined on the reaper thread
    void addThread ( const std::shared_ptr<Thread>& thread );

    // Start the event manager for polling, doesn't block
    void startPolling();

    // Poll for events instead of start / stop, returns false on exit
    bool poll();

    // Start the event manager, blocks until stop is called
    void start();

    // Stop the event manager
    void stop();

    // Stop the event manager and release background threads
    void release();

    // Get the singleton instance
    static EventManager& get();
};
