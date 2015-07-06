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

private:

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
};
