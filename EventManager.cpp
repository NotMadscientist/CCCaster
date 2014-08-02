#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "Logger.h"

#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

#include <cassert>

using namespace std;


#define DEFAULT_TIMEOUT_MILLISECONDS 1000


void EventManager::checkEvents ( double timeout )
{
    TimerManager::get().check();

    if ( TimerManager::get().getNextExpiry() != UINT64_MAX )
        timeout = TimerManager::get().getNextExpiry() - TimerManager::get().getNow();

    assert ( timeout > 0 );

    SocketManager::get().check ( timeout );

    ControllerManager::get().check();
}

void EventManager::eventLoop()
{
    if ( TimerManager::get().isHiRes() )
    {
        while ( running )
        {
            Sleep ( 1 );
            checkEvents ( DEFAULT_TIMEOUT_MILLISECONDS );
        }
    }
    else
    {
        timeBeginPeriod ( 1 );

        while ( running )
        {
            Sleep ( 1 );
            checkEvents ( DEFAULT_TIMEOUT_MILLISECONDS );
        }

        timeEndPeriod ( 1 );
    }
}

EventManager::EventManager() : running ( false )
{
}

bool EventManager::poll ( double timeout )
{
    if ( !running )
        return false;

    assert ( timeout > 0 );

    TimerManager::get().updateNow();
    double now = TimerManager::get().getNow();
    double end = now + timeout;

    while ( now < end )
    {
        checkEvents ( end - now );

        if ( !running )
            break;

        TimerManager::get().updateNow();
        now = TimerManager::get().getNow();
    }

    if ( running )
        return true;

    LOG ( "Finished polling" );

    LOG ( "Joining reaper thread" );

    reaperThread.join();

    LOG ( "Joined reaper thread" );

    return false;
}

void EventManager::startPolling()
{
    running = true;

    LOG ( "Starting polling" );
}

void EventManager::start()
{
    running = true;

    LOG ( "Starting event loop" );

    eventLoop();

    LOG ( "Finished event loop" );

    LOG ( "Joining reaper thread" );

    reaperThread.join();

    LOG ( "Joined reaper thread" );
}

void EventManager::stop()
{
    LOG ( "Stopping everything" );

    running = false;
}

void EventManager::release()
{
    stop();

    LOG ( "Releasing everything" );

    reaperThread.release();
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}

void EventManager::ReaperThread::run()
{
    for ( ;; )
    {
        shared_ptr<Thread> thread = zombieThreads.pop();

        LOG ( "Joining %08x", thread.get() );

        if ( thread )
            thread->join();
        else
            return;

        LOG ( "Joined %08x", thread.get() );
    }
}

void EventManager::ReaperThread::join()
{
    zombieThreads.push ( shared_ptr<Thread>() );
    Thread::join();
    zombieThreads.clear();
}

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    reaperThread.zombieThreads.push ( thread );
}
