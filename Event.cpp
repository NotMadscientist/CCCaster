#include "Event.h"
#include "Log.h"

#include <cassert>

using namespace std;

EventManager::EventManager()
    : running ( false )
    , reaperThread ( zombieThreads )
    , socketAcceptCmd ( socketMap )
    , socketDisconnectCmd ( socketMap )
    , socketReadCmd ( socketMap )
    , timerThread ( *this )
{
    socketGroup.setCmdOnAccept ( &socketAcceptCmd );
    socketGroup.setCmdOnDisconnect ( &socketDisconnectCmd );
    socketGroup.setCmdOnRead ( &socketReadCmd );
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

void EventManager::stop()
{
    LOG ( "Stopping everything" );

    LOCK ( mutex );
    running = false;
    socketsCond.signal();
    timersCond.signal();
    timerThread.release();
    reaperThread.release();
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
