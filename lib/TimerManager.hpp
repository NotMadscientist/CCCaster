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
    bool isInitialized() const { return _initialized; }

    // Indicates if using the hi-res timer
    bool isHiRes() const { return _useHiResTimer; }

    // Get the current time in milliseconds
    uint64_t getNow() const { return _now; }
    uint64_t getNow ( bool update ) { if ( update ) updateNow(); return _now; }

    // Get the next time when a timer will expire
    uint64_t getNextExpiry() const { return _nextExpiry; }

    // Get the singleton instance
    static TimerManager& get();

private:

    // Sets of active and allocated timer instances
    std::unordered_set<Timer *> _activeTimers, _allocatedTimers;

    // Indicates if the hi-res timer should be used
    bool _useHiResTimer;

    // Hi-res timer variables
    uint64_t _ticksPerSecond = 0, _ticks = 0;

    // The current time in milliseconds
    uint64_t _now = 0;

    // The next time when a timer will expire
    uint64_t _nextExpiry = 0;

    // Flag to indicate the set of allocated timers has changed
    bool _changed = false;

    // Flag to indicate if initialized
    bool _initialized = false;

    // Private constructor, etc. for singleton class
    TimerManager();
    TimerManager ( const TimerManager& );
    const TimerManager& operator= ( const TimerManager& );
};
