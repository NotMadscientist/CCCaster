#include "Event.h"
#include "Socket.h"
#include "Timer.h"
#include "Log.h"

#include <winsock2.h>
#include <windows.h>

#include <cassert>

#define SELECT_TIMEOUT_MICROSECONDS 100

using namespace std;

bool EventManager::initialized = false;

void EventManager::eventLoop()
{
    // Seed the RNG in this thread because Windows has per-thread RNG
    srand ( time ( 0 ) );

    // Initialize timer in this thread because it is thread dependent
    bool useHiRes = true;
    LARGE_INTEGER ticksPerSecond, ticks;
    {
        // Make sure we are using a single core on a dual core machine, otherwise timings will be off.
        DWORD_PTR oldMask = SetThreadAffinityMask ( GetCurrentThread(), 1 );

        // Check if the hi-res timer is supported
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

            QueryPerformanceCounter ( &ticks );
            now = 1000 * ticks.QuadPart / ticksPerSecond.QuadPart;

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

    activeSockets.clear();
}

void EventManager::checkSockets()
{
    for ( Socket *socket : allocatedSockets )
    {
        if ( activeSockets.find ( socket ) != activeSockets.end() )
            continue;

        LOG_SOCKET ( "Added", socket );
        activeSockets.insert ( socket );
    }

    for ( auto it = activeSockets.begin(); it != activeSockets.end(); )
    {
        if ( allocatedSockets.find ( *it ) != allocatedSockets.end() )
        {
            ++it;
            continue;
        }

        LOG ( "Removed socket %08x", *it ); // Don't log any extra data cus already dealloc'd
        activeSockets.erase ( it++ );
    }

    fd_set readFds, writeFds;
    FD_ZERO ( &readFds );
    FD_ZERO ( &writeFds );

    for ( Socket *socket : activeSockets )
    {
        if ( socket->state == Socket::State::Connecting )
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
        LOG ( "select failed: %s", getLastWinSockError().c_str() );
        throw "something"; // TODO
    }

    if ( count == 0 )
        return;

    for ( Socket *socket : activeSockets )
    {
        if ( allocatedSockets.find ( socket ) == allocatedSockets.end() )
            continue;

        if ( socket->state == Socket::State::Connecting )
        {
            if ( !FD_ISSET ( socket->fd, &writeFds ) )
                continue;

            LOG_SOCKET ( "Connected", socket );
            socket->state = Socket::State::Connected;
            socket->owner->connectEvent ( socket );
        }
        else
        {
            if ( !FD_ISSET ( socket->fd, &readFds ) )
                continue;

            if ( socket->isServer() && socket->protocol == Socket::Protocol::TCP )
            {
                LOG_SOCKET ( "Accept from server", socket );
                socket->owner->acceptEvent ( socket );
            }
            else
            {
                u_long numBytes;

                if ( ioctlsocket ( socket->fd, FIONREAD, &numBytes ) != 0 )
                {
                    LOG ( "ioctlsocket failed: %s", getLastWinSockError().c_str() );
                    throw "something"; // TODO
                }

                if ( socket->protocol == Socket::Protocol::TCP && numBytes == 0 )
                {
                    LOG_SOCKET ( "Disconnected", socket );
                    Socket::Owner *owner = socket->owner;
                    socket->disconnect();
                    owner->disconnectEvent ( socket );
                }
                else
                {
                    LOG_SOCKET ( "Read from", socket );

                    char *bufferEnd = & ( socket->readBuffer[socket->readPos] );
                    size_t bufferLen = socket->readBuffer.size() - socket->readPos;

                    IpAddrPort address = socket->getRemoteAddress();
                    bool success = false;

                    if ( socket->protocol == Socket::Protocol::TCP )
                        success = socket->recv ( bufferEnd, bufferLen );
                    else
                        success = socket->recv ( bufferEnd, bufferLen, address );

                    if ( !success )
                    {
                        // Disconnect the socket if an error occured during read
                        LOG_SOCKET ( "Disconnected", socket );

                        if ( socket->protocol == Socket::Protocol::TCP )
                        {
                            Socket::Owner *owner = socket->owner;
                            socket->disconnect();
                            owner->disconnectEvent ( socket );
                        }
                        else
                        {
                            socket->disconnect();
                        }

                        continue;
                    }

                    // Simulated packet loss
                    if ( rand() % 100 < socket->packetLoss )
                    {
                        LOG ( "Discarding [ %u bytes ] from '%s'", bufferLen, address.c_str() );
                        continue;
                    }

                    // Increment the buffer position
                    socket->readPos += bufferLen;
                    LOG ( "Read [ %u bytes ] from '%s'", bufferLen, address.c_str() );

                    // Handle zero byte packets
                    if ( bufferLen == 0 )
                    {
                        LOG ( "Decoded [ 0 bytes ] to 'NullMsg'" );
                        socket->owner->readEvent ( socket, NullMsg, address );
                        continue;
                    }

                    LOG ( "Base64 : %s", toBase64 ( bufferEnd, bufferLen ).c_str() );

                    // Try to decode as many messages from the buffer as possible
                    for ( ;; )
                    {
                        size_t consumed = 0;
                        MsgPtr msg = Serializable::decode ( & ( socket->readBuffer[0] ), socket->readPos, consumed );

                        if ( !msg.get() )
                            break;

                        LOG ( "Decoded [ %u bytes ] to '%s'", consumed, TO_C_STR ( msg ) );
                        socket->owner->readEvent ( socket, msg, address );

                        // Abort if the socket is no longer alive
                        if ( allocatedSockets.find ( socket ) == allocatedSockets.end() )
                            break;

                        assert ( consumed <= socket->readPos );

                        // Erase the consumed bytes (shifting the array)
                        socket->readBuffer.erase ( 0, consumed );
                        socket->readPos -= consumed;
                    }
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

        LOG ( "Removed timer %08x", *it ); // Don't log any extra data cus already dealloc'd
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

            timer->owner->timerExpired ( timer );
            timer->expiry = 0;
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
    LOG ( "Adding timer %08x; delay='%llu ms'", timer, timer->delay );
    allocatedTimers.insert ( timer );
}

void EventManager::removeTimer ( Timer *timer )
{
    LOG ( "Removing timer %08x", timer );
    allocatedTimers.erase ( timer );
}

void EventManager::addSocket ( Socket *socket )
{
    LOG_SOCKET ( "Adding", socket );
    allocatedSockets.insert ( socket );
}

void EventManager::removeSocket ( Socket *socket )
{
    LOG_SOCKET ( "Removing", socket );
    allocatedSockets.erase ( socket );
}

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    reaperThread.zombieThreads.push ( thread );
}


EventManager::EventManager() : now ( 0 ), running ( false )
{
}

EventManager::~EventManager()
{
}

void EventManager::initialize()
{
    if ( initialized )
        return;

    WSADATA wsaData;
    int error = WSAStartup ( MAKEWORD ( 2, 2 ), &wsaData );

    if ( error != NO_ERROR )
    {
        LOG ( "WSAStartup failed: %s", getWindowsErrorAsString ( error ).c_str() );
        throw "something"; // TODO
    }

    initialized = true;
}

void EventManager::start()
{
    assert ( initialized == true );

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

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
