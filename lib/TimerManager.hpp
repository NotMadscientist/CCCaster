#pragma once

#include <unordered_set>


class Timer;


class TimerManager
{
public:

    // Update current time
    void updateNow();

    // Check for timer events
    void check();

    // Add / remove / clear timer instances
    void add ( Timer *timer );
    void remove ( Timer *timer );
    void clear();

    // Initialize / deinitialize timer manager
    void initialize();
    void deinitialize();
    bool isInitialized() const { return initialized; }

    // Indicates if using the hi-res timer
    bool isHiRes() const { return useHiResTimer; }

    // Get the current time in milliseconds
    uint64_t getNow() const { return now; }
    uint64_t getNow ( bool update ) { if ( update ) updateNow(); return now; }

    // Get the next time when a timer will expire
    uint64_t getNextExpiry() const { return nextExpiry; }

    // Get the singleton instance
    static TimerManager& get();

private:

    // Sets of active and allocated timer instances
    std::unordered_set<Timer *> activeTimers, allocatedTimers;

    // Indicates if the hi-res timer should be used
    bool useHiResTimer;

    // Hi-res timer variables
    uint64_t ticksPerSecond = 0, ticks = 0;

    // The current time in milliseconds
    uint64_t now = 0;

    // The next time when a timer will expire
    uint64_t nextExpiry = 0;

    // Flag to indicate the set of allocated timers has changed
    bool changed = false;

    // Flag to indicate if initialized
    bool initialized = false;

    // Private constructor, etc. for singleton class
    TimerManager();
    TimerManager ( const TimerManager& );
    const TimerManager& operator= ( const TimerManager& );
};
