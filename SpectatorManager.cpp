#include "SpectatorManager.h"
#include "NetplayManager.h"
#include "ProcessManager.h"
#include "Logger.h"
#include "Algorithms.h"
#include "Constants.h"

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

void SpectatorManager::pushSpectator ( Socket *socketPtr, const IpAddrPort& serverAddr )
{
    LOG ( "socket=%08x; serverAddr='%s'", socketPtr, serverAddr );

    SocketPtr newSocket = popPendingSocket ( socketPtr );

    if ( !newSocket )
        return;

    ASSERT ( newSocket.get() == socketPtr );

    // Add new spectators just AFTER the current spectator position.
    // This way whenever a new spectator causes a decrease in the broadcast interval, later spectators
    // will still get their next set of inputs late enough that they don't need to wait another interval.
    //
    // Example:
    //
    // Spectators: [2] 1        interval is 15/2 = 7, so #1 will get a broadcast in 7 frames
    //
    // Spectators: [2] 3 1      interval is 15/3 = 5, now #1 will get a broadcast in 10 frames
    //
    // Spectators: 2 [3] 1      interval is 15/3 = 5, so #1 will get a broadcast in 5 frames
    //
    // Spectators: 2 [3] 4 1    interval is 15/4 = 3, now #1 will get a broadcast in 6 frames
    //
    list<Socket *>::iterator it;

    if ( spectatorList.empty() )
        it = spectatorList.insert ( spectatorList.end(), socketPtr );
    else if ( spectatorListPos == spectatorList.end() )
        it = spectatorList.insert ( spectatorList.begin(), socketPtr );
    else
        it = spectatorList.insert ( incremented ( spectatorListPos ), socketPtr );

    Spectator spectator;
    spectator.socket = newSocket;
    spectator.serverAddr = serverAddr;
    spectator.it = it;
    spectator.pos.parts.frame = NUM_INPUTS - 1;
    spectator.pos.parts.index = netManPtr->getSpectateStartIndex();

    spectatorMap[socketPtr] = spectator;

    if ( spectatorMap.size() == 1 || spectatorMapPos == spectatorMap.end() )
        spectatorMapPos = spectatorMap.begin();

    netManPtr->preserveStartIndex = min ( netManPtr->preserveStartIndex, spectator.pos.parts.index );

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

    if ( spectatorMapPos == it )
        ++spectatorMapPos;

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
    {
        spectatorListPos = spectatorList.end();
        spectatorMapPos = spectatorMap.end();

        // Reset the preserve index
        netManPtr->preserveStartIndex = currentMinIndex = UINT_MAX;
        return;
    }

    if ( spectatorMapPos == spectatorMap.end() )
        spectatorMapPos = spectatorMap.begin();

    if ( spectatorMap.size() > 1 )
        ++spectatorMapPos;

    if ( spectatorMapPos == spectatorMap.end() )
        spectatorMapPos = spectatorMap.begin();

    // Number of times to broadcast per frame
    const uint32_t multiplier = 1 + ( spectatorList.size() * 2 ) / ( NUM_INPUTS + 1 );

    // Number of frames between each broadcast
    const uint32_t interval = ( multiplier * NUM_INPUTS / 2 ) / spectatorList.size();

    if ( ( *CC_WORLD_TIMER_ADDR ) % interval )
        return;

    for ( uint32_t i = 0; i < multiplier; ++i )
    {
        // Once we reach the end
        if ( spectatorListPos == spectatorList.end() )
        {
            // Restart from the beginning
            spectatorListPos = spectatorList.begin();

            // Update the preserve index
            netManPtr->preserveStartIndex = currentMinIndex;

            // Reset the current min index
            currentMinIndex = UINT_MAX;
        }

        const auto it = spectatorMap.find ( *spectatorListPos );

        ASSERT ( it != spectatorMap.end() );

        Socket *socket = it->first;
        Spectator& spectator = it->second;
        const uint32_t oldIndex = spectator.pos.parts.index;

        LOG ( "socket=%08x; spectator.pos=[%s]", socket, spectator.pos );

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

        // Update the current min index
        currentMinIndex = min ( currentMinIndex, spectator.pos.parts.index );
    }
}

const IpAddrPort& SpectatorManager::getRandomSpectatorAddress() const
{
    if ( spectatorMap.empty() || spectatorMapPos == spectatorMap.end() )
    {
        LOG ( "'%s'", NullAddress );
        return NullAddress;
    }

    LOG ( "'%s'", spectatorMapPos->second.serverAddr );
    return spectatorMapPos->second.serverAddr;
}
