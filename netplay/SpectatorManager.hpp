#pragma once

#include "Timer.hpp"
#include "Socket.hpp"
#include "Constants.hpp"

#include <unordered_map>
#include <list>


// Default pending socket timeout
#define DEFAULT_PENDING_TIMEOUT ( 20000 )


// Forward declarations
struct RngState;
struct NetplayManager;
struct ProcessManager;


struct Spectator
{
    SocketPtr socket;

    IndexedFrame pos = {{ 0, 0 }};

    bool sentRngState = false, sentRetryMenuIndex = false;

    IpAddrPort serverAddr;

    std::list<Socket *>::iterator it;
};


class SpectatorManager
{
public:

    // Timeout for pending sockets, ie sockets that have been accepted but not doing anything yet.
    // Changing this value will only affect newly accepted sockets; already accepted sockets are unaffected.
    uint64_t pendingSocketTimeout = DEFAULT_PENDING_TIMEOUT;


    SpectatorManager();

    SpectatorManager ( NetplayManager *netManPtr, const ProcessManager *procManPtr );


    bool isPendingSocket ( Socket *socket ) const { return ( _pendingSockets.find ( socket ) != _pendingSockets.end() ); }

    void pushPendingSocket ( Timer::Owner *owner, const SocketPtr& socket );

    SocketPtr popPendingSocket ( Socket *socket );

    void timerExpired ( Timer *timer );


    size_t numSpectators() const { return _spectatorMap.size(); }

    void pushSpectator ( Socket *socket, const IpAddrPort& serverAddr );

    void popSpectator ( Socket *socket );

    const IpAddrPort& getRandomSpectatorAddress() const;


    void newRngState ( const RngState& rngState );

    void frameStepSpectators();

private:

    std::unordered_map<Socket *, SocketPtr> _pendingSockets;

    std::unordered_map<Socket *, TimerPtr> _pendingSocketTimers;

    std::unordered_map<Timer *, Socket *> _pendingTimerToSocket;

    std::unordered_map<Socket *, Spectator> _spectatorMap;

    std::list<Socket *> _spectatorList;

    std::list<Socket *>::iterator _spectatorListPos;

    std::unordered_map<Socket *, Spectator>::const_iterator _spectatorMapPos;

    uint32_t _currentMinIndex = UINT_MAX;

    NetplayManager *_netManPtr = 0;

    const ProcessManager *_procManPtr = 0;
};
