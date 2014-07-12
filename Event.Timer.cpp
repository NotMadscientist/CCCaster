#include "Event.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>
#include <mmsystem.h>

using namespace std;

void EventManager::updateTime()
{
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

void EventManager::checkTimers()
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

        LOG ( "Removed timer %08x", *it ); // Don't log any extra data cus already deleted
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

void EventManager::addTimer ( Timer *timer )
{
    LOG ( "Adding timer %s; delay='%llu ms'", timer, timer->delay );
    allocatedTimers.insert ( timer );
}

void EventManager::removeTimer ( Timer *timer )
{
    LOG ( "Removing timer %08x", timer );
    allocatedTimers.erase ( timer );
}

void EventManager::clearTimers()
{
    LOG ( "clearTimers" );
    activeTimers.clear();
    allocatedTimers.clear();
}

void EventManager::initializeTimers()
{
    if ( initializedTimers )
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

    initializedTimers = true;
}

void EventManager::deinitializeTimers()
{
    initializedTimers = false;
}
