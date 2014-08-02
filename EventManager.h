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

    // Single instance of the reaper thread, to garbage collect finished thread
    ReaperThread reaperThread;

    // Flag to indicate the event loop is running
    volatile bool running;

    // Check for events
    void checkEvents ( double timeout );

    // Main event loop
    void eventLoop();

    // Private constructor, etc. for singleton class
    EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add a thread to be joined on the reaper thread, aka garbage collected when it finishes
    void addThread ( const std::shared_ptr<Thread>& thread );

    // Start the EventManager for polling, doesn't block
    void startPolling();

    // Poll for events instead of start / stop, returns false if the EventManager has been stopped
    bool poll ( double timeout );

    // Start the EventManager, blocks until stop is called
    void start();

    // Stop the EventManager, can be called on a different thread
    void stop();

    // Stop the EventManager and release background threads, can be called on a different thread
    void release();

    // Get the singleton instance
    static EventManager& get();
};
