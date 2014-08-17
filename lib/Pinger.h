#pragma once

#include "Timer.h"
#include "Protocol.h"
#include "Statistics.h"


struct Ping : public SerializableMessage
{
    uint64_t timestamp;

    Ping() {}
    Ping ( uint64_t timestamp ) : timestamp ( timestamp ) {}

    PROTOCOL_BOILERPLATE ( timestamp );
};


class Pinger : public Timer::Owner
{
public:

    struct Owner
    {
        virtual void sendPing ( Pinger *pinger, const MsgPtr& ping ) {}

        virtual void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) {};
    };

    Owner *owner = 0;

private:

    TimerPtr pingTimer;

    uint64_t pingInterval;

    size_t pingCount, numPings;

    Statistics stats;

    bool pinging = false;

    void timerExpired ( Timer *timer ) override;

public:

    Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings );

    void start();

    void gotPong ( const MsgPtr& ping );

    const Statistics& getStats() const { return stats; }
};
