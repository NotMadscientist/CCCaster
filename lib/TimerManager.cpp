#include "TimerManager.hpp"
#include "Timer.hpp"
#include "Logger.hpp"

#include <windows.h>
#include <mmsystem.h>

using namespace std;


void TimerManager::updateNow()
{
    if ( ! _initialized )
        return;

    if ( _useHiResTimer )
    {
        QueryPerformanceCounter ( ( LARGE_INTEGER * ) &_ticks );
        _now = ( 1000 * _ticks ) / _ticksPerSecond;
    }
    else
    {
        // Note: timeGetTime should be called between timeBeginPeriod / timeEndPeriod to ensure accuracy
        _now = timeGetTime();
    }
}

void TimerManager::check()
{
    if ( ! _initialized )
        return;

    if ( _changed )
    {
        for ( Timer *timer : _allocatedTimers )
        {
            if ( _activeTimers.find ( timer ) != _activeTimers.end() )
                continue;

            LOG ( "Added timer %08x; delay='%llu ms'", timer, timer->_delay );
            _activeTimers.insert ( timer );
        }

        for ( auto it = _activeTimers.begin(); it != _activeTimers.end(); )
        {
            if ( _allocatedTimers.find ( *it ) != _allocatedTimers.end() )
            {
                ++it;
                continue;
            }

            LOG ( "Removed timer %08x", *it ); // Don't log any extra data cus already de-allocated
            _activeTimers.erase ( it++ );
        }

        _changed = false;
    }

    _nextExpiry = UINT64_MAX;

    if ( _activeTimers.empty() )
        return;

    updateNow();

    for ( Timer *timer : _activeTimers )
    {
        if ( _allocatedTimers.find ( timer ) == _allocatedTimers.end() )
            continue;

        if ( timer->_expiry > 0 && _now >= timer->_expiry )
        {
            LOG ( "Expired timer %08x", timer );

            timer->_delay = timer->_expiry = 0;

            if ( timer->owner )
            {
                timer->owner->timerExpired ( timer );

                if ( _allocatedTimers.find ( timer ) == _allocatedTimers.end() )
                    continue;
            }
        }

        if ( timer->_delay > 0 )
        {
            LOG ( "Started timer %08x; delay='%llu ms'", timer, timer->_delay );

            timer->_expiry = _now + timer->_delay;
            timer->_delay = 0;
        }

        if ( timer->_expiry > 0 && timer->_expiry < _nextExpiry )
            _nextExpiry = timer->_expiry;
    }
}

void TimerManager::add ( Timer *timer )
{
    LOG ( "Adding timer %08x", timer );

    _allocatedTimers.insert ( timer );
    _changed = true;
}

void TimerManager::remove ( Timer *timer )
{
    if ( _allocatedTimers.erase ( timer ) )
    {
        LOG ( "Removing timer %08x", timer );

        _changed = true;
    }
}

void TimerManager::clear()
{
    LOG ( "Clearing timers" );

    _activeTimers.clear();
    _allocatedTimers.clear();
    _changed = true;
}

TimerManager::TimerManager() : _useHiResTimer ( true ) {}

void TimerManager::initialize()
{
    if ( _initialized )
        return;

    _initialized = true;

    // Seed the RNG in this thread because Windows has per-thread RNG, and timers are also thread specific
    srand ( time ( 0 ) );

    // Make sure we are using a single core on a dual core machine, otherwise timings will be off.
    DWORD_PTR oldMask = SetThreadAffinityMask ( GetCurrentThread(), 1 );

    // Check if the hi-res timer is supported
    if ( ! QueryPerformanceFrequency ( ( LARGE_INTEGER * ) &_ticksPerSecond ) )
    {
        LOG ( "Hi-res timer not supported" );

        _useHiResTimer = false;

        SetThreadAffinityMask ( GetCurrentThread(), oldMask );
    }
}

void TimerManager::deinitialize()
{
    if ( ! _initialized )
        return;

    _initialized = false;

    TimerManager::get().clear();
}

TimerManager& TimerManager::get()
{
    static TimerManager instance;
    return instance;
}
