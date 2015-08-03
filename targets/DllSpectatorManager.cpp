#include "SpectatorManager.hpp"
#include "DllNetplayManager.hpp"
#include "ProcessManager.hpp"
#include "Logger.hpp"
#include "Algorithms.hpp"
#include "Constants.hpp"

using namespace std;


SpectatorManager::SpectatorManager ( NetplayManager *netManPtr, const ProcessManager *procManPtr )
    : _spectatorListPos ( _spectatorList.end() )
    , _spectatorMapPos ( _spectatorMap.end() )
    , _netManPtr ( netManPtr )
    , _procManPtr ( procManPtr )
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

    if ( _spectatorList.empty() )
        it = _spectatorList.insert ( _spectatorList.end(), socketPtr );
    else if ( _spectatorListPos == _spectatorList.end() )
        it = _spectatorList.insert ( _spectatorList.begin(), socketPtr );
    else
        it = _spectatorList.insert ( incremented ( _spectatorListPos ), socketPtr );

    Spectator spectator;
    spectator.socket = newSocket;
    spectator.serverAddr = serverAddr;
    spectator.it = it;
    spectator.pos.parts.frame = NUM_INPUTS - 1;
    spectator.pos.parts.index = _netManPtr->getSpectateStartIndex();

    _spectatorMap[socketPtr] = spectator;

    if ( _spectatorMap.size() == 1 || _spectatorMapPos == _spectatorMap.cend() )
        _spectatorMapPos = _spectatorMap.cbegin();

    _netManPtr->preserveStartIndex = min ( _netManPtr->preserveStartIndex, spectator.pos.parts.index );

    LOG ( "socket=%08x; spectator.pos=[%s]; preserveStartIndex=%u",
          socketPtr, spectator.pos, _netManPtr->preserveStartIndex );

    const uint8_t netplayState = _netManPtr->getState().value;
    const bool isTraining = _netManPtr->config.mode.isTraining();

    switch ( netplayState )
    {
        case NetplayState::CharaSelect:
            newSocket->send ( _netManPtr->getRngState ( spectator.pos.parts.index ) );
            break;

        case NetplayState::Skippable:
        case NetplayState::InGame:
        case NetplayState::RetryMenu:
            newSocket->send ( _netManPtr->getRngState ( spectator.pos.parts.index + ( isTraining ? 1 : 2 ) ) );
            break;
    }

    newSocket->send ( new InitialGameState ( spectator.pos, netplayState, isTraining ) );
}

void SpectatorManager::popSpectator ( Socket *socketPtr )
{
    LOG ( "socket=%08x", socketPtr );

    const auto it = _spectatorMap.find ( socketPtr );

    if ( it == _spectatorMap.end() )
        return;

    if ( _spectatorListPos == it->second.it )
        ++_spectatorListPos;

    if ( _spectatorMapPos == it )
        ++_spectatorMapPos;

    _spectatorList.erase ( it->second.it );
    _spectatorMap.erase ( socketPtr );
}

void SpectatorManager::newRngState ( const RngState& rngState )
{
    for ( Socket *socket : _spectatorList )
        socket->send ( rngState );
}

void SpectatorManager::frameStepSpectators()
{
    if ( _spectatorMap.empty() )
    {
        _spectatorListPos = _spectatorList.end();
        _spectatorMapPos = _spectatorMap.cend();

        // Reset the preserve index
        _netManPtr->preserveStartIndex = _currentMinIndex = UINT_MAX;
        return;
    }

    if ( _spectatorMapPos == _spectatorMap.cend() )
        _spectatorMapPos = _spectatorMap.cbegin();

    if ( _spectatorMap.size() > 1 )
        ++_spectatorMapPos;

    if ( _spectatorMapPos == _spectatorMap.cend() )
        _spectatorMapPos = _spectatorMap.cbegin();

    // Number of times to broadcast per frame
    const uint32_t multiplier = 1 + ( _spectatorList.size() * 2 ) / ( NUM_INPUTS + 1 );

    // Number of frames between each broadcast
    const uint32_t interval = ( multiplier * NUM_INPUTS / 2 ) / _spectatorList.size();

    if ( ( *CC_WORLD_TIMER_ADDR ) % interval )
        return;

    for ( uint32_t i = 0; i < multiplier; ++i )
    {
        // Once we reach the end
        if ( _spectatorListPos == _spectatorList.end() )
        {
            // Restart from the beginning
            _spectatorListPos = _spectatorList.begin();

            // Update the preserve index
            _netManPtr->preserveStartIndex = _currentMinIndex;

            // Reset the current min index
            _currentMinIndex = UINT_MAX;
        }

        const auto it = _spectatorMap.find ( *_spectatorListPos );

        ASSERT ( it != _spectatorMap.end() );

        Socket *socket = it->first;
        Spectator& spectator = it->second;
        const uint32_t oldIndex = spectator.pos.parts.index;

        LOG ( "socket=%08x; spectator.pos=[%s]; preserveStartIndex=%u",
              socket, spectator.pos, _netManPtr->preserveStartIndex );

        MsgPtr msgBothInputs = _netManPtr->getBothInputs ( spectator.pos );

        // Send inputs if available
        if ( msgBothInputs )
            socket->send ( msgBothInputs );

        // Clear sent flags whenever the index changes
        if ( spectator.pos.parts.index > oldIndex )
        {
            spectator.sentRngState = false;
            spectator.sentRetryMenuIndex = false;
        }

        MsgPtr msgRngState = _netManPtr->getRngState ( oldIndex );

        // Send RngState ONCE if available
        if ( msgRngState && !spectator.sentRngState )
        {
            socket->send ( msgRngState );
            spectator.sentRngState = true;
        }

        MsgPtr msgMenuIndex = _netManPtr->getRetryMenuIndex ( oldIndex );

        // Send retry menu index ONCE if available
        if ( msgMenuIndex && !spectator.sentRetryMenuIndex )
        {
            socket->send ( msgMenuIndex );
            spectator.sentRetryMenuIndex = true;
        }

        ++_spectatorListPos;

        // Update the current min index
        _currentMinIndex = min ( _currentMinIndex, spectator.pos.parts.index );
    }
}

const IpAddrPort& SpectatorManager::getRandomSpectatorAddress() const
{
    if ( _spectatorMap.empty() || _spectatorMapPos == _spectatorMap.cend() )
    {
        LOG ( "'%s'", NullAddress );
        return NullAddress;
    }

    auto it = _spectatorMapPos;

#ifndef RELEASE
    if ( it->second.serverAddr.port == 0 )
    {
        do
        {
            ++it;

            if ( it == _spectatorMap.end() )
                it = _spectatorMap.begin();
        }
        while ( it->second.serverAddr.port == 0 && it != _spectatorMapPos );
    }
#endif

    LOG ( "'%s'", it->second.serverAddr );
    return it->second.serverAddr;
}
