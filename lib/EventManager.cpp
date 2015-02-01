#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "Logger.h"

#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

using namespace std;


#define DEFAULT_TIMEOUT_MILLISECONDS ( 1000 )


void EventManager::checkEvents ( uint64_t timeout )
{
    if ( !running )
        return;

    ASSERT ( TimerManager::get().isInitialized() == true );
    ASSERT ( SocketManager::get().isInitialized() == true );

    TimerManager::get().check();

    if ( !running )
        return;

    if ( TimerManager::get().getNextExpiry() != UINT64_MAX )
    {
        uint64_t newTimeout = 1;

        if ( TimerManager::get().getNextExpiry() > TimerManager::get().getNow() )
            newTimeout = TimerManager::get().getNextExpiry() - TimerManager::get().getNow();

        if ( newTimeout < timeout )
            timeout = newTimeout;
    }

    ASSERT ( timeout > 0 );

    SocketManager::get().check ( timeout );
}

void EventManager::eventLoop()
{
    if ( TimerManager::get().isHiRes() )
    {
        timeBeginPeriod ( 1 ); // for select, see comment in SocketManager

        while ( running )
        {
            Sleep ( 1 );
            checkEvents ( DEFAULT_TIMEOUT_MILLISECONDS );
        }

        timeEndPeriod ( 1 ); // for select, see comment in SocketManager
    }
    else
    {
        timeBeginPeriod ( 1 ); // for timeGetTime AND select

        while ( running )
        {
            Sleep ( 1 );
            checkEvents ( DEFAULT_TIMEOUT_MILLISECONDS );
        }

        timeEndPeriod ( 1 ); // for timeGetTime AND select
    }
}

EventManager::EventManager() {}

bool EventManager::poll ( uint64_t timeout )
{
    if ( !running )
        return false;

    ASSERT ( timeout > 0 );

    uint64_t now = TimerManager::get().getNow ( true );
    const uint64_t end = now + timeout;

    timeBeginPeriod ( 1 ); // for select, see comment in SocketManager

    while ( now < end )
    {
        checkEvents ( end - now );

        if ( !running )
            break;

        now = TimerManager::get().getNow ( true );
    }

    timeEndPeriod ( 1 ); // for select, see comment in SocketManager

    if ( running )
        return true;

    LOG ( "Finished polling" );

    // LOG ( "Joining reaper thread" );
    // reaperThread.join();
    // LOG ( "Joined reaper thread" );

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

    stop();
}

void EventManager::stop()
{
    LOG ( "Stopping everything" );

    running = false;

    // LOG ( "Joining reaper thread" );
    // reaperThread.join();
    // LOG ( "Joined reaper thread" );
}

void EventManager::release()
{
    LOG ( "Releasing everything" );

    running = false;

    LOG ( "Releasing reaper thread" );

    reaperThread.release();
}

EventManager& EventManager::get()
{
    static EventManager instance;
    return instance;
}

void EventManager::ReaperThread::run()
{
    for ( ;; )
    {
        ThreadPtr thread = zombieThreads.pop();

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
    zombieThreads.push ( ThreadPtr() );
    Thread::join();
    zombieThreads.clear();
}

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    reaperThread.zombieThreads.push ( thread );
}
