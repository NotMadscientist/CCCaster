#include "Event.h"
#include "Log.h"

#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

using namespace std;

void EventManager::checkEvents()
{
    updateTime();
    checkTimers();
    checkSockets();
    checkJoysticks();
}

void EventManager::eventLoop()
{
    if ( useHiResTimer )
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

EventManager::EventManager()
    : useHiResTimer ( true ), now ( 0 ), running ( false )
    , initializedSockets ( false ), initializedTimers ( false ), initializedJoysticks ( false )
{
}

EventManager::~EventManager()
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

    clearTimers();
    clearSockets();
    clearJoysticks();

    LOG ( "Joining reaper thread" );

    reaperThread.join();

    LOG ( "Joined reaper thread" );

    return false;
}

void EventManager::start()
{
    running = true;

    LOG ( "Starting event loop" );

    eventLoop();

    LOG ( "Finished event loop" );

    clearTimers();
    clearSockets();
    clearJoysticks();

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

void EventManager::initialize()
{
    initializeTimers();
    initializeSockets();
    initializeJoysticks();
}

void EventManager::initializePolling()
{
    initialize();

    running = true;
}

void EventManager::deinitialize()
{
    deinitializeTimers();
    deinitializeSockets();
    deinitializeJoysticks();
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
