#include "SpectatorManager.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"

using namespace std;


SpectatorManager::SpectatorManager() {}

void SpectatorManager::pushPendingSocket ( Timer::Owner *owner, const SocketPtr& socket )
{
    LOG ( "socket=%08x", socket.get() );

    TimerPtr timer ( new Timer ( owner ) );
    timer->start ( pendingSocketTimeout );

    _pendingSockets[socket.get()] = socket;
    _pendingSocketTimers[socket.get()] = timer;
    _pendingTimerToSocket[timer.get()] = socket.get();
}

SocketPtr SpectatorManager::popPendingSocket ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    const auto it = _pendingSockets.find ( socketPtr );

    if ( it == _pendingSockets.end() )
        return 0;

    ASSERT ( _pendingSocketTimers.find ( socketPtr ) != _pendingSocketTimers.end() );

    SocketPtr socket = it->second;
    Timer *timerPtr = _pendingSocketTimers[socketPtr].get();

    ASSERT ( _pendingTimerToSocket.find ( timerPtr ) != _pendingTimerToSocket.end() );

    _pendingTimerToSocket.erase ( timerPtr );
    _pendingSocketTimers.erase ( socketPtr );
    _pendingSockets.erase ( socketPtr );

    return socket;
}

void SpectatorManager::timerExpired ( Timer *timerPtr )
{
    LOG ( "timer=%08x", timerPtr );

    const auto it = _pendingTimerToSocket.find ( timerPtr );

    if ( it == _pendingTimerToSocket.end() )
        return;

    LOG ( "socket=%08x", it->second );

    ASSERT ( _pendingSockets.find ( it->second ) != _pendingSockets.end() );
    ASSERT ( _pendingSocketTimers.find ( it->second ) != _pendingSocketTimers.end() );

    _pendingSocketTimers.erase ( it->second );
    _pendingSockets.erase ( it->second );
    _pendingTimerToSocket.erase ( timerPtr );
}
