#include "SpectatorManager.h"
#include "NetplayManager.h"
#include "ProcessManager.h"
#include "Logger.h"

using namespace std;


SpectatorManager::SpectatorManager ( NetplayManager *netManPtr, const ProcessManager *procManPtr )
    : spectatorListPos ( spectatorList.end() )
    , spectatorMapPos ( spectatorMap.end() )
    , netManPtr ( netManPtr )
    , procManPtr ( procManPtr )
{
}

void SpectatorManager::pushPendingSocket ( Timer::Owner *owner, const SocketPtr& socket )
{
    LOG ( "socket=%08x", socket.get() );

    TimerPtr timer ( new Timer ( owner ) );
    timer->start ( pendingSocketTimeout );

    pendingSockets[socket.get()] = socket;
    pendingSocketTimers[socket.get()] = timer;
    pendingTimerToSocket[timer.get()] = socket.get();
}

SocketPtr SpectatorManager::popPendingSocket ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    const auto it = pendingSockets.find ( socketPtr );

    if ( it == pendingSockets.end() )
        return 0;

    ASSERT ( pendingSocketTimers.find ( socketPtr ) != pendingSocketTimers.end() );

    SocketPtr socket = it->second;
    Timer *timerPtr = pendingSocketTimers[socketPtr].get();

    ASSERT ( pendingTimerToSocket.find ( timerPtr ) != pendingTimerToSocket.end() );

    pendingTimerToSocket.erase ( timerPtr );
    pendingSocketTimers.erase ( socketPtr );
    pendingSockets.erase ( socketPtr );

    return socket;
}

void SpectatorManager::timerExpired ( Timer *timerPtr )
{
    LOG ( "timer=%08x", timerPtr );

    const auto it = pendingTimerToSocket.find ( timerPtr );

    if ( it == pendingTimerToSocket.end() )
        return;

    LOG ( "socket=%08x", it->second );

    ASSERT ( pendingSockets.find ( it->second ) != pendingSockets.end() );
    ASSERT ( pendingSocketTimers.find ( it->second ) != pendingSocketTimers.end() );

    pendingSocketTimers.erase ( it->second );
    pendingSockets.erase ( it->second );
    pendingTimerToSocket.erase ( timerPtr );
}
