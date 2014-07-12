#include "TimerManager.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>
#include <mmsystem.h>

using namespace std;

void TimerManager::update()
{
    if ( !initialized )
        return;

    if ( useHiResTimer )
    {
        QueryPerformanceCounter ( ( LARGE_INTEGER * ) &ticks );
        now = 1000L * ticks / ticksPerSecond;
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

    for ( Timer *timer : activeTimers )
    {
        if ( allocatedTimers.find ( timer ) == allocatedTimers.end() )
            continue;

        if ( timer->delay > 0 )
        {
            LOG ( "Started timer %08x; delay='%llu ms'", timer, timer->delay );

            timer->expiry = now + timer->delay;
            timer->delay = 0;
        }
        else if ( timer->expiry > 0 && now >= timer->expiry )
        {
            LOG ( "Expired timer %08x", timer );

            timer->delay = timer->expiry = 0;
            timer->owner->timerExpired ( timer );
        }
    }
}

void TimerManager::add ( Timer *timer )
{
    LOG ( "Adding timer %s; delay='%llu ms'", timer, timer->delay );
    allocatedTimers.insert ( timer );
}

void TimerManager::remove ( Timer *timer )
{
    LOG ( "Removing timer %08x", timer );
    allocatedTimers.erase ( timer );
}

void TimerManager::clear()
{
    LOG ( "Clearing timers" );
    activeTimers.clear();
    allocatedTimers.clear();
}

TimerManager::TimerManager() : initialized ( false ) {}

void TimerManager::initialize()
{
    if ( initialized )
        return;

    // Seed the RNG in this thread because Windows has per-thread RNG
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
    initialized = false;
}

TimerManager& TimerManager::get()
{
    static TimerManager tm;
    return tm;
}
