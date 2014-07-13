#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "Log.h"

#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

using namespace std;

void EventManager::checkEvents()
{
    TimerManager::get().update();
    TimerManager::get().check();
    SocketManager::get().check();
    ControllerManager::get().check();
}

void EventManager::eventLoop()
{
    if ( TimerManager::get().isHiRes() )
    {
        while ( running )
        {
            Sleep ( 1 );
            checkEvents();
        }
    }
    else
    {
        timeBeginPeriod ( 1 );

        while ( running )
        {
            Sleep ( 1 );
            checkEvents();
        }

        timeEndPeriod ( 1 );
    }
}

EventManager::EventManager() : running ( false )
{
}

bool EventManager::poll()
{
    if ( !running )
        return false;

    checkEvents();

    if ( running )
        return true;

    LOG ( "Finished polling" );

    ControllerManager::get().clear();
    SocketManager::get().clear();
    TimerManager::get().clear();

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

    ControllerManager::get().clear();
    SocketManager::get().clear();
    TimerManager::get().clear();

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
