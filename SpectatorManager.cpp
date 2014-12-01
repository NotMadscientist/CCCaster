#include "SpectatorManager.h"
#include "NetplayManager.h"
#include "ProcessManager.h"
#include "Logger.h"
#include "Algorithms.h"

using namespace std;


SpectatorManager::SpectatorManager ( const NetplayManager *netManPtr, const ProcessManager *procManPtr )
    : spectatorPos ( spectatorList.end() )
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

    const auto it = spectatorList.insert ( spectatorList.end(), socketPtr );

    Spectator spectator;
    spectator.socket = newSocket;
    spectator.it = it;

    if ( netManPtr->getState() == NetplayState::CharaSelect || netManPtr->getState() == NetplayState::Loading )
    {
        // // When spectating before InGame, we can sync the complete game state, so start on current frame.
        // spectator.pos = netManPtr->getIndexedFrame();

        // // Send the current RngState
        // newSocket->send ( procManPtr->getRngState ( spectator.pos.parts.index -
        //                   ( netManPtr->getState() == NetplayState::CharaSelect ? 0 : 1 ) ) );
    }
    else
    {
        // Otherwise we must start from the beginning of the current game.
        spectator.pos = { 0, netManPtr->getGameStartIndex() };
    }

    newSocket->send ( new InitialGameState (
                          spectator.pos, netManPtr->getState().value, netManPtr->config.mode.isTraining() ) );

    spectatorMap[socketPtr] = spectator;
}

void SpectatorManager::popSpectator ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    const auto it = spectatorMap.find ( socketPtr );

    if ( it == spectatorMap.end() )
        return;

    if ( spectatorPos == it->second.it )
        ++spectatorPos;

    spectatorList.erase ( it->second.it );
    spectatorMap.erase ( socketPtr );
}

void SpectatorManager::newRngState ( const RngState& rngState )
{
    for ( Socket *socket : spectatorList )
        socket->send ( rngState );
}

void SpectatorManager::broadcastFrameStep()
{
    if ( spectatorMap.empty() )
        return;

    // const uint32_t interval = clamped ( ( NUM_INPUTS / spectatorList.size() / 2 ), 1, NUM_INPUTS );

    if ( netManPtr->getFrame() % ( NUM_INPUTS / 2 ) )
        return;

    if ( spectatorPos == spectatorList.end() )
        spectatorPos = spectatorList.begin();

    auto it = spectatorMap.find ( *spectatorPos );

    ASSERT ( it != spectatorMap.end() );

    Socket *socket = it->first;
    Spectator& spectator = it->second;

    MsgPtr msgBothInputs = netManPtr->getBothInputs ( spectator.pos );

    if ( msgBothInputs )
        socket->send ( msgBothInputs );

    ++spectatorPos;
}
