#include "SpectatorManager.h"
#include "Logger.h"

using namespace std;


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

    auto it = pendingSockets.find ( socketPtr );

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

    auto it = pendingTimerToSocket.find ( timerPtr );

    if ( it == pendingTimerToSocket.end() )
        return;

    LOG ( "socket=%08x", it->second );

    ASSERT ( pendingSockets.find ( it->second ) != pendingSockets.end() );
    ASSERT ( pendingSocketTimers.find ( it->second ) != pendingSocketTimers.end() );

    pendingSocketTimers.erase ( it->second );
    pendingSockets.erase ( it->second );
    pendingTimerToSocket.erase ( timerPtr );
}

void SpectatorManager::pushSpectator ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    SocketPtr newSocket = popPendingSocket ( socketPtr );

    if ( !newSocket )
        return;

    ASSERT ( newSocket.get() == socketPtr );

    Spectator spectator;
    spectator.socket = newSocket;

    // if ( netMan.getState() == NetplayState::CharaSelect || netMan.getState() == NetplayState::Loading )
    // {
    //     // When spectating before InGame, we can sync the complete game state, so start on current frame.
    //     spectator.pos = netMan.getIndexedFrame();
    // }
    // else
    // {
    //     // Otherwise we must start from the beginning of the current game.
    //     spectator.pos = { 0, gameStartIndex };
    // }

    // newSocket->send ( new InitialGameState ( spectator.pos.parts.index, netMan.getState().value ) );

    // if ( netMan.getState() == NetplayState::CharaSelect )
    //     newSocket->send ( procMan.getRngState ( 0 ) );

    spectators[socketPtr] = spectator;
}

void SpectatorManager::popSpectator ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    spectators.erase ( socketPtr );
}

void SpectatorManager::newRngState ( const RngState& rngState )
{
    for ( const auto& kv : spectators )
        kv.first->send ( rngState );
}

void SpectatorManager::broadcastFrameStep ( const NetplayManager& netMan )
{
}
