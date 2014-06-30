#include "Event.h"
#include "Timer.h"
#include "Log.h"
#include "Util.h"

#include <windows.h>

#include <vector>

using namespace std;

static uint64_t now = 0;

string Timer::formatTimer ( const Timer *timer )
{
    return toString ( "%08x:'%llu ms'", timer, timer->delay );
}

void EventManager::addTimer ( Timer *timer )
{
    LOG ( "Adding timer %08x; delay='%llu ms'", timer, timer->delay );

    LOCK ( mutex );

//     LOG_LIST ( activeTimers, Timer::formatTimer );

    activeTimers.insert ( timer );

    timersCond.signal();
}

void EventManager::removeTimer ( Timer *timer )
{
    LOG ( "Removing timer %08x", timer );

    LOCK ( mutex );

    activeTimers.erase ( timer );

//     LOG_LIST ( activeTimers, Timer::formatTimer );
}

void EventManager::TimerThread::start()
{
    if ( running )
        return;

    running = true;
    Thread::start();
}

void EventManager::TimerThread::join()
{
    if ( !running )
        return;

    running = false;
    Thread::join();
}

void EventManager::TimerThread::run()
{
    LARGE_INTEGER ticksPerSecond, ticks;

    if ( useHiRes )
    {
        // Make sure we are using a single core on a dual core machine, otherwise timings will be off.
        DWORD_PTR oldMask = SetThreadAffinityMask ( GetCurrentThread(), 1 );

        if ( !QueryPerformanceFrequency ( &ticksPerSecond ) )
        {
            LOG ( "Hi-res timer not supported" );
            useHiRes = false;
            SetThreadAffinityMask ( GetCurrentThread(), oldMask );
        }
    }

    EventManager& em = EventManager::get();

    if ( useHiRes )
    {
        while ( running )
        {
            Sleep ( 1 );

            Lock lock ( em.mutex );

            while ( em.activeTimers.empty() && em.running )
                em.timersCond.wait ( em.mutex );

            if ( !em.running )
                break;

            QueryPerformanceCounter ( &ticks );
            now = 1000 * ticks.QuadPart / ticksPerSecond.QuadPart;

            checkTimers();
        }
    }
    else
    {
        timeBeginPeriod ( 1 );

        while ( running )
        {
            Sleep ( 1 );

            Lock lock ( em.mutex );

            while ( em.activeTimers.empty() && em.running )
                em.timersCond.wait ( em.mutex );

            if ( !em.running )
                break;

            now = timeGetTime();

            checkTimers();
        }

        timeEndPeriod ( 1 );
    }

    Lock lock ( em.mutex );
    em.activeTimers.clear();
}

void EventManager::TimerThread::checkTimers()
{
    EventManager& em = EventManager::get();

    vector<Timer *> toRemove;

    for ( Timer *timer : em.activeTimers )
    {
        if ( timer->delay > 0 )
        {
            LOG ( "Started timer %08x; delay='%llu ms'", timer, timer->delay );

            timer->expiry = now + timer->delay;
            timer->delay = 0;
        }
        else if ( now >= timer->expiry )
        {
            LOG ( "Expired timer %08x", timer );

            timer->owner->timerExpired ( timer );
            timer->expiry = 0;

            if ( timer->delay == 0 )
                toRemove.push_back ( timer );
        }

    }

    for ( Timer *timer : toRemove )
    {
        LOG ( "Removed timer %08x", timer );

        em.activeTimers.erase ( timer );

//         LOG_LIST ( em.activeTimers, Timer::formatTimer );
    }
}
