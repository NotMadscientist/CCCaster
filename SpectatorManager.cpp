#include "SpectatorManager.h"
#include "NetplayManager.h"
#include "ProcessManager.h"
#include "Logger.h"
#include "Algorithms.h"

using namespace std;


SpectatorManager::SpectatorManager ( const NetplayManager *netManPtr, const ProcessManager *procManPtr )
    : spectatorListPos ( spectatorList.end() )
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
    spectator.pos.parts.frame = NUM_INPUTS - 1;
    spectator.pos.parts.index = netManPtr->getSpectateStartIndex();

    spectatorMap[socketPtr] = spectator;

    const uint8_t netplayState = netManPtr->getState().value;
    const bool isTraining = netManPtr->config.mode.isTraining();

    switch ( netplayState )
    {
        case NetplayState::CharaSelect:
            newSocket->send ( netManPtr->getRngState ( spectator.pos.parts.index ) );
            break;

        case NetplayState::Skippable:
        case NetplayState::InGame:
        case NetplayState::RetryMenu:
            newSocket->send ( netManPtr->getRngState ( spectator.pos.parts.index + ( isTraining ? 1 : 2 ) ) );
            break;
    }

    newSocket->send ( new InitialGameState ( spectator.pos, netplayState, isTraining ) );
}

void SpectatorManager::popSpectator ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    const auto it = spectatorMap.find ( socketPtr );

    if ( it == spectatorMap.end() )
        return;

    if ( spectatorListPos == it->second.it )
        ++spectatorListPos;

    spectatorList.erase ( it->second.it );
    spectatorMap.erase ( socketPtr );
}

void SpectatorManager::newRngState ( const RngState& rngState )
{
    for ( Socket *socket : spectatorList )
        socket->send ( rngState );
}

void SpectatorManager::frameStepSpectators()
{
    if ( spectatorMap.empty() )
        return;

    // const uint32_t interval = clamped ( ( NUM_INPUTS / spectatorList.size() / 2 ), 1, NUM_INPUTS );

    // TODO fix this interval
    if ( netManPtr->getFrame() % ( NUM_INPUTS / 2 ) )
        return;

    if ( spectatorListPos == spectatorList.end() )
        spectatorListPos = spectatorList.begin();

    auto it = spectatorMap.find ( *spectatorListPos );

    ASSERT ( it != spectatorMap.end() );

    Socket *socket = it->first;
    Spectator& spectator = it->second;
    const uint32_t oldIndex = spectator.pos.parts.index;

    LOG ( "spectator.pos=[%s]", spectator.pos );

    MsgPtr msgBothInputs = netManPtr->getBothInputs ( spectator.pos );

    // Send inputs if available
    if ( msgBothInputs )
        socket->send ( msgBothInputs );

    // Clear sent flags whenever the index changes
    if ( spectator.pos.parts.index > oldIndex )
    {
        spectator.sentRngState = false;
        spectator.sentRetryMenuIndex = false;
    }

    MsgPtr msgRngState = netManPtr->getRngState ( oldIndex );

    // Send RngState ONCE if available
    if ( msgRngState && !spectator.sentRngState )
    {
        socket->send ( msgRngState );
        spectator.sentRngState = true;
    }

    MsgPtr msgMenuIndex = netManPtr->getRetryMenuIndex ( oldIndex );

    // Send retry menu index ONCE if available
    if ( msgMenuIndex && !spectator.sentRetryMenuIndex )
    {
        socket->send ( msgMenuIndex );
        spectator.sentRetryMenuIndex = true;
    }

    ++spectatorListPos;
}
