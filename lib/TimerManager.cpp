#include "TimerManager.h"
#include "Timer.h"
#include "Logger.h"

#include <windows.h>
#include <mmsystem.h>

using namespace std;


void TimerManager::updateNow()
{
    if ( !initialized )
        return;

    if ( useHiResTimer )
    {
        QueryPerformanceCounter ( ( LARGE_INTEGER * ) &ticks );
        now = 1000 * ticks / ticksPerSecond;
    }
    else
    {
        now = timeGetTime();
    }
}

void TimerManager::check()
{
    if ( !initialized )
        return;

    if ( changed )
    {
        for ( Timer *timer : allocatedTimers )
        {
            if ( activeTimers.find ( timer ) != activeTimers.end() )
                continue;

            LOG ( "Added timer %08x; delay='%llu ms'", timer, timer->delay );
            activeTimers.insert ( timer );
        }

        for ( auto it = activeTimers.begin(); it != activeTimers.end(); )
        {
            if ( allocatedTimers.find ( *it ) != allocatedTimers.end() )
            {
                ++it;
                continue;
            }

            LOG ( "Removed timer %08x", *it ); // Don't log any extra data cus already de-allocated
            activeTimers.erase ( it++ );
        }

        changed = false;
    }

    nextExpiry = UINT64_MAX;

    if ( activeTimers.empty() )
        return;

    updateNow();

    for ( Timer *timer : activeTimers )
    {
        if ( allocatedTimers.find ( timer ) == allocatedTimers.end() )
            continue;

        if ( timer->expiry > 0 && now >= timer->expiry )
        {
            LOG ( "Expired timer %08x", timer );

            timer->delay = timer->expiry = 0;

            if ( timer->owner )
            {
                timer->owner->timerExpired ( timer );

                if ( allocatedTimers.find ( timer ) == allocatedTimers.end() )
                    continue;
            }
        }

        if ( timer->delay > 0 )
        {
            LOG ( "Started timer %08x; delay='%llu ms'", timer, timer->delay );

            timer->expiry = now + timer->delay;
            timer->delay = 0;
        }

        if ( timer->expiry < nextExpiry )
            nextExpiry = timer->expiry;
    }

    if ( nextExpiry < UINT64_MAX )
        LOG ( "nextExpiry in %llu ms", nextExpiry - now );
}

void TimerManager::add ( Timer *timer )
{
    LOG ( "Adding timer %08x; delay='%llu ms'", timer, timer->delay );
    allocatedTimers.insert ( timer );
    changed = true;
}

void TimerManager::remove ( Timer *timer )
{
    if ( allocatedTimers.erase ( timer ) )
    {
        LOG ( "Removing timer %08x", timer );
        changed = true;
    }
}

void TimerManager::clear()
{
    LOG ( "Clearing timers" );
    activeTimers.clear();
    allocatedTimers.clear();
    changed = true;
}

TimerManager::TimerManager() : useHiResTimer ( true ) {}

void TimerManager::initialize()
{
    if ( initialized )
        return;

    // Seed the RNG in this thread because Windows has per-thread RNG, and timers are also thread specific
    srand ( time ( 0 ) );

    // Make sure we are using a single core on a dual core machine, otherwise timings will be off.
    DWORD_PTR oldMask = SetThreadAffinityMask ( GetCurrentThread(), 1 );

    // Check if the hi-res timer is supported
    if ( !QueryPerformanceFrequency ( ( LARGE_INTEGER * ) &ticksPerSecond ) )
    {
        LOG ( "Hi-res timer not supported" );
        useHiResTimer = false;
        SetThreadAffinityMask ( GetCurrentThread(), oldMask );
    }

    initialized = true;
}

void TimerManager::deinitialize()
{
    if ( !initialized )
        return;

    TimerManager::get().clear();
    initialized = false;
}

TimerManager& TimerManager::get()
{
    static TimerManager instance;
    return instance;
}
