#include "Event.h"
#include "Log.h"

#include <cassert>

using namespace std;

EventManager::EventManager()
    : running ( false )
    , reaperThread ( zombieThreads )
{
    socketGroup.setCmdOnAccept ( &sac );
    socketGroup.setCmdOnDisconnect ( &sdc );
    socketGroup.setCmdOnRead ( &srd );
}

EventManager::~EventManager()
{
}

void EventManager::start()
{
    running = true;

    LOG ( "Starting timer thread" );

    timerThread.start();

    LOG ( "Starting listen loop" );

    socketListenLoop();
}

void EventManager::stop ( bool release )
{
    LOG ( "Stopping everything" );

    LOCK ( mutex );
    running = false;
    socketsCond.signal();
    timersCond.signal();

    if ( release )
    {
        timerThread.release();
        reaperThread.release();
    }
    else
    {
        timerThread.join();
        reaperThread.join();
    }
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
