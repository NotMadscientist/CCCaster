#pragma once

#include "Thread.hpp"
#include "BlockingQueue.hpp"

#include <memory>


#define CHECK_TIMERS        0x0001
#define CHECK_SOCKETS       0x0002
#define CHECK_CONTROLLERS   0x0004


class EventManager
{
    // Thread to join zombie thread
    struct ReaperThread : public Thread
    {
        // Finished threads to kill
        BlockingQueue<ThreadPtr> zombieThreads;

        // Thread functions
        void run() override;
        void join() override;
    };

    // Single instance of the reaper thread, to garbage collect finished thread
    ReaperThread reaperThread;

    // Flag to indicate the event loop is running
    volatile bool running = false;

    // Check for events
    void checkEvents ( uint64_t timeout );

    // Main event loop
    void eventLoop();

    // Private constructor, etc. for singleton class
    EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add a thread to be joined on the reaper thread, aka garbage collected when it finishes
    void addThread ( const ThreadPtr& thread );

    // Start the EventManager for polling, doesn't block
    void startPolling();

    // Poll for events instead of start / stop, returns false if the EventManager has been stopped
    bool poll ( uint64_t timeout );

    // Start the EventManager, blocks until stop is called
    void start();

    // Stop the EventManager, can be called on a different thread
    void stop();

    // Stop the EventManager and release background threads, can be called on a different thread
    void release();

    // Indicate the EventManager is running
    bool isRunning() const { return running; }

    // Get the singleton instance
    static EventManager& get();
};
