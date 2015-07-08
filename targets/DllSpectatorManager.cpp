#include "SpectatorManager.hpp"
#include "DllNetplayManager.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"
#include "Algorithms.hpp"
#include "Constants.hpp"

using namespace std;


SpectatorManager::SpectatorManager ( NetplayManager *netManPtr, const ProcessManager *procManPtr )
    : spectatorListPos ( spectatorList.end() )
    , spectatorMapPos ( spectatorMap.end() )
    , netManPtr ( netManPtr )
    , procManPtr ( procManPtr )
{
}

void SpectatorManager::pushSpectator ( Socket *socketPtr, const IpAddrPort& serverAddr )
{
    LOG ( "socket=%08x; serverAddr='%s'", socketPtr, serverAddr );

    SocketPtr newSocket = popPendingSocket ( socketPtr );

    if ( ! newSocket )
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

    if ( spectatorMap.size() == 1 || spectatorMapPos == spectatorMap.cend() )
        spectatorMapPos = spectatorMap.cbegin();

    netManPtr->preserveStartIndex = min ( netManPtr->preserveStartIndex, spectator.pos.parts.index );

    LOG ( "socket=%08x; spectator.pos=[%s]; preserveStartIndex=%u",
          socketPtr, spectator.pos, netManPtr->preserveStartIndex );

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
        spectatorMapPos = spectatorMap.cend();

        // Reset the preserve index
        netManPtr->preserveStartIndex = currentMinIndex = UINT_MAX;
        return;
    }

    if ( spectatorMapPos == spectatorMap.cend() )
        spectatorMapPos = spectatorMap.cbegin();

    if ( spectatorMap.size() > 1 )
        ++spectatorMapPos;

    if ( spectatorMapPos == spectatorMap.cend() )
        spectatorMapPos = spectatorMap.cbegin();

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

        LOG ( "socket=%08x; spectator.pos=[%s]; preserveStartIndex=%u",
              socket, spectator.pos, netManPtr->preserveStartIndex );

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
    if ( spectatorMap.empty() || spectatorMapPos == spectatorMap.cend() )
    {
        LOG ( "'%s'", NullAddress );
        return NullAddress;
    }

    auto it = spectatorMapPos;

#ifndef RELEASE
    if ( it->second.serverAddr.port == 0 )
    {
        do
        {
            ++it;

            if ( it == spectatorMap.end() )
                it = spectatorMap.begin();
        }
        while ( it->second.serverAddr.port == 0 && it != spectatorMapPos );
    }
#endif

    LOG ( "'%s'", it->second.serverAddr );
    return it->second.serverAddr;
}
