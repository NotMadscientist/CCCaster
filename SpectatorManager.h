#pragma once

#include "Timer.h"
#include "Socket.h"
#include "Constants.h"

#include <unordered_map>
#include <list>


// Default pending socket timeout
#define DEFAULT_PENDING_TIMEOUT ( 10000 )

// Forward declaration
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
    std::unordered_map<Socket *, SocketPtr> pendingSockets;

    std::unordered_map<Socket *, TimerPtr> pendingSocketTimers;

    std::unordered_map<Timer *, Socket *> pendingTimerToSocket;

    std::unordered_map<Socket *, Spectator> spectatorMap;

    std::list<Socket *> spectatorList;

    std::list<Socket *>::iterator spectatorListPos;

    std::unordered_map<Socket *, Spectator>::const_iterator spectatorMapPos;

    uint32_t currentMinIndex = UINT_MAX;

    NetplayManager *netManPtr = 0;

    const ProcessManager *procManPtr = 0;

public:

    // Timeout for pending sockets, ie sockets that have been accepted but not doing anything yet.
    // Changing this value will only affect newly accepted sockets; already accepted sockets are unaffected.
    uint64_t pendingSocketTimeout = DEFAULT_PENDING_TIMEOUT;


    SpectatorManager ( NetplayManager *netManPtr = 0, const ProcessManager *procManPtr = 0 );


    bool isPendingSocket ( Socket *socket ) const { return ( pendingSockets.find ( socket ) != pendingSockets.end() ); }

    void pushPendingSocket ( Timer::Owner *owner, const SocketPtr& socket );

    SocketPtr popPendingSocket ( Socket *socket );

    void timerExpired ( Timer *timer );


    size_t numSpectators() const { return spectatorMap.size(); }

    void pushSpectator ( Socket *socket, const IpAddrPort& serverAddr );

    void popSpectator ( Socket *socket );

    const IpAddrPort& getRandomSpectatorAddress() const;


    void newRngState ( const RngState& rngState );

    void frameStepSpectators();
};
