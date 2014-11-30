#pragma once

#include "Timer.h"
#include "Socket.h"
#include "NetplayManager.h"

#include <unordered_map>


// Default pending socket timeout
#define DEFAULT_PENDING_TIMEOUT ( 10000 )

// Forward declaration
struct RngState;


struct Spectator
{
    SocketPtr socket;

    IndexedFrame pos = {{ 0, 0 }};
};


class SpectatorManager
{
    std::unordered_map<Socket *, SocketPtr> pendingSockets;

    std::unordered_map<Socket *, TimerPtr> pendingSocketTimers;

    std::unordered_map<Timer *, Socket *> pendingTimerToSocket;

    std::unordered_map<Socket *, Spectator> spectators;

public:

    // Timeout for pending sockets, ie sockets that have been accepted but not doing anything yet.
    // Changing this value will only affect newly accepted sockets; already accepted sockets are unaffected.
    uint64_t pendingSocketTimeout = DEFAULT_PENDING_TIMEOUT;


    void pushPendingSocket ( Timer::Owner *owner, const SocketPtr& socket );

    SocketPtr popPendingSocket ( Socket *socket );

    void timerExpired ( Timer *timer );


    void pushSpectator ( Socket *socket );

    void popSpectator ( Socket *socket );


    void newRngState ( const RngState& rngState );

    void broadcastFrameStep ( const NetplayManager& netMan );
};
