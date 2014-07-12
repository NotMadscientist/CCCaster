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

    // Flags to indicate the event manager is initialized
    bool initializedSockets;
    bool initializedTimers;
    bool initializedJoysticks;

    // Update time
    void updateTime();

    // Check and update events
    void checkEvents();
    void checkTimers();
    void checkSockets();
    void checkJoysticks();

    // Main event loop
    void eventLoop();

    // Private constructor, etc. for singleon class
    EventManager();
    ~EventManager();
    EventManager ( const EventManager& );
    const EventManager& operator= ( const EventManager& );

public:

    // Add / remove / clear timer instances
    void addTimer ( Timer *timer );
    void removeTimer ( Timer *timer );
    void clearTimers();

    // Add / remove / clear socket instances
    void addSocket ( Socket *socket );
    void removeSocket ( Socket *socket );
    void clearSockets();

    // Clear all joystick instances
    void clearJoysticks();

    // Add a thread to be joined on the reaper thread
    void addThread ( const std::shared_ptr<Thread>& thread );

    // Poll for events instead of start / stop, returns false on exit
    bool poll();

    // Start the event manager, blocks until stop is called
    void start();

    // Stop the event manager
    void stop();

    // Stop the event manager and release background threads
    void release();

    // Initialize / deinitialize the event manager, should be called in the same thread as start / poll
    void initialize();
    void deinitialize();
    void initializePolling();

    // Initialize / deinitialize specific subsystems, only timer is thread dependent
    void initializeTimers();
    void initializeSockets();
    void initializeJoysticks();
    void deinitializeTimers();
    void deinitializeSockets();
    void deinitializeJoysticks();

    // Get the current time in milliseconds
    inline uint64_t getNow() const { return now; }

    // Get the singleton instance
    static EventManager& get();
};
