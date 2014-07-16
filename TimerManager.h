#pragma once

#include <unordered_set>

class Timer;

class TimerManager
{
    // Sets of active and allocated timer instances
    std::unordered_set<Timer *> activeTimers, allocatedTimers;

    // Indicates if the hi-res timer should be used
    bool useHiResTimer;

    // Hi-res timer variables
    uint64_t ticksPerSecond, ticks;

    // The current time in milliseconds
    uint64_t now;

    // The next time when a timer will expire
    uint64_t nextExpiry;

    // Flag to inidicate if initialized
    bool initialized;

    // Private constructor, etc. for singleton class
    TimerManager();
    TimerManager ( const TimerManager& );
    const TimerManager& operator= ( const TimerManager& );

public:

    // Update current time
    void updateCurrentTime();

    // Check for timer events
    void check();

    // Add / remove / clear timer instances
    void add ( Timer *timer );
    void remove ( Timer *timer );
    void clear();

    // Initialize / deinitialize timer manager
    void initialize();
    void deinitialize();
    inline bool isInitialized() const { return initialized; }

    // Indicates if using the hi-res timer
    inline bool isHiRes() const { return useHiResTimer; }

    // Get the current time in milliseconds
    inline uint64_t getNow() const { return now; }

    // Get the next time when a timer will expire
    inline uint64_t getNextExpiry() const { return nextExpiry; }

    // Get the singleton instance
    static TimerManager& get();
};
