#include "Event.h"
#include "Socket.h"
#include "Timer.h"
#include "Log.h"

#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

#include <cassert>

#define SELECT_TIMEOUT_MICROSECONDS 100

using namespace std;

void EventManager::eventLoop()
{
    if ( useHiResTimer )
    {
        while ( running )
        {
            Sleep ( 1 );

            QueryPerformanceCounter ( ( LARGE_INTEGER * ) &ticks );
            now = 1000L * ticks / ticksPerSecond;

            checkTimers();

            checkSockets();
        }
    }
    else
    {
        timeBeginPeriod ( 1 );

        while ( running )
        {
            Sleep ( 1 );

            now = timeGetTime();

            checkTimers();

            checkSockets();
        }

        timeEndPeriod ( 1 );
    }

    activeTimers.clear();
    allocatedTimers.clear();

    activeSockets.clear();
    allocatedSockets.clear();
}

void EventManager::checkSockets()
{
    for ( Socket *socket : allocatedSockets )
    {
        if ( activeSockets.find ( socket ) != activeSockets.end() )
            continue;

        LOG_SOCKET ( socket, "added" );
        activeSockets.insert ( socket );
    }

    for ( auto it = activeSockets.begin(); it != activeSockets.end(); )
    {
        if ( allocatedSockets.find ( *it ) != allocatedSockets.end() )
        {
            ++it;
            continue;
        }

        LOG ( "socket %08x removed", *it ); // Don't log any extra data cus already deleted
        activeSockets.erase ( it++ );
    }

    if ( activeSockets.empty() )
        return;

    fd_set readFds, writeFds;
    FD_ZERO ( &readFds );
    FD_ZERO ( &writeFds );

    for ( Socket *socket : activeSockets )
    {
        if ( socket->isConnecting() && socket->isTCP() )
            FD_SET ( socket->fd, &writeFds );
        else
            FD_SET ( socket->fd, &readFds );
    }

    timeval timeout;
    timeout.tv_sec = SELECT_TIMEOUT_MICROSECONDS / ( 1000 * 1000 );
    timeout.tv_usec = SELECT_TIMEOUT_MICROSECONDS % ( 1000 * 1000 );

    int count = select ( 0, &readFds, &writeFds, 0, &timeout );

    if ( count == SOCKET_ERROR )
    {
        WindowsError err = WSAGetLastError();
        LOG ( "select failed: %s", err );
        throw err;
    }

    if ( count == 0 )
        return;

    for ( Socket *socket : activeSockets )
    {
        if ( allocatedSockets.find ( socket ) == allocatedSockets.end() )
            continue;

        if ( socket->isConnecting() && socket->isTCP() )
        {
            if ( !FD_ISSET ( socket->fd, &writeFds ) )
                continue;

            LOG_SOCKET ( socket, "connectEvent" );
            socket->connectEvent();
        }
        else
        {
            if ( !FD_ISSET ( socket->fd, &readFds ) )
                continue;

            if ( socket->isServer() && socket->isTCP() )
            {
                LOG_SOCKET ( socket, "acceptEvent" );
                socket->acceptEvent();
            }
            else
            {
                u_long numBytes;

                if ( ioctlsocket ( socket->fd, FIONREAD, &numBytes ) != 0 )
                {
                    WindowsError err = WSAGetLastError();
                    LOG ( "ioctlsocket failed: %s", err );
                    throw err;
                }

                if ( socket->isTCP() && numBytes == 0 )
                {
                    LOG_SOCKET ( socket, "disconnectEvent" );
                    socket->disconnectEvent();
                }
                else
                {
                    LOG_SOCKET ( socket, "readEvent" );
                    socket->readEvent();
                }
            }
        }
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

void EventManager::addSocket ( Socket *socket )
{
    LOG_SOCKET ( socket, "addSocket" );
    allocatedSockets.insert ( socket );
}

void EventManager::removeSocket ( Socket *socket )
{
    LOG_SOCKET ( socket, "removeSocket" );
    allocatedSockets.erase ( socket );
}

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    reaperThread.zombieThreads.push ( thread );
}


EventManager::EventManager() : useHiResTimer ( true ), now ( 0 ), running ( false ), initialized ( false )
{
}

EventManager::~EventManager()
{
}

void EventManager::start()
{
    running = true;

    LOG ( "Starting event loop" );

    eventLoop();

    LOG ( "Finished event loop" );

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

bool EventManager::poll()
{
    if ( useHiResTimer )
    {
        QueryPerformanceCounter ( ( LARGE_INTEGER * ) &ticks );
        now = 1000L * ticks / ticksPerSecond;

        checkTimers();

        checkSockets();
    }
    else
    {
        timeBeginPeriod ( 1 );

        now = timeGetTime();

        checkTimers();

        checkSockets();

        timeEndPeriod ( 1 );
    }

    if ( running )
        return true;

    activeTimers.clear();
    allocatedTimers.clear();

    activeSockets.clear();
    allocatedSockets.clear();

    return false;
}

void EventManager::initialize()
{
    if ( initialized )
        return;

    // Initialize WinSock
    WSADATA wsaData;
    int error = WSAStartup ( MAKEWORD ( 2, 2 ), &wsaData );

    if ( error != NO_ERROR )
    {
        WindowsError err = error;
        LOG ( "WSAStartup failed: %s", err );
        throw err;
    }

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

void EventManager::deinitialize()
{
    if ( !initialized )
        return;

    WSACleanup();

    initialized = false;
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
