#include "Event.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>

#include <vector>

using namespace std;

static double now = 0.0;

void EventManager::addTimer ( Timer *timer )
{
    timerSet.insert ( timer );
}

void EventManager::removeTimer ( Timer *timer )
{
    timerSet.erase ( timer );
}

double EventManager::now() const { return ::now; }

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

    if ( useHiRes )
    {
        while ( running )
        {
            Sleep ( 1 );

            Lock lock ( context.mutex );

            while ( context.timerSet.empty() && context.running )
                context.timersCond.wait ( context.mutex );

            if ( !context.running )
                break;

            QueryPerformanceCounter ( &ticks );
            ::now = 1000.0 * ticks.QuadPart / ticksPerSecond.QuadPart;

            checkTimers ( ::now );
        }
    }
    else
    {
        timeBeginPeriod ( 1 );

        while ( running )
        {
            Sleep ( 1 );

            Lock lock ( context.mutex );

            while ( context.timerSet.empty() && context.running )
                context.timersCond.wait ( context.mutex );

            if ( !context.running )
                break;

            ::now = timeGetTime();

            checkTimers ( ::now );
        }

        timeEndPeriod ( 1 );
    }
}

void EventManager::TimerThread::checkTimers ( double now )
{
    vector<Timer *> toRemove;

    for ( Timer *timer : context.timerSet )
    {
        if ( now >= timer->expiry )
        {
            Lock lock ( context.mutex );
            timer->owner.timerExpired ( timer );

            if ( now >= timer->expiry )
                toRemove.push_back ( timer );
        }
    }

    for ( Timer *timer : toRemove )
        context.timerSet.erase ( timer );
}
