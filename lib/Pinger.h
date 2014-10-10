#pragma once

#include "Timer.h"
#include "Protocol.h"
#include "Statistics.h"


struct Ping : public SerializableMessage
{
    uint64_t timestamp;

    Ping ( uint64_t timestamp ) : timestamp ( timestamp ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( Ping, timestamp );
};


class Pinger : public Timer::Owner
{
public:

    struct Owner
    {
        virtual void sendPing ( Pinger *pinger, const MsgPtr& ping ) = 0;

        virtual void donePinging ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) = 0;
    };

    Owner *owner = 0;

private:

    TimerPtr pingTimer;

    size_t pingCount = 0;

    Statistics stats;

    uint8_t packetLoss = 0;

    bool pinging = false;

    void timerExpired ( Timer *timer ) override;

public:

    uint64_t pingInterval = 0;

    size_t numPings = 0;

    Pinger();

    Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings );

    void start();

    void gotPong ( const MsgPtr& ping );

    const Statistics& getStats() const { return stats; }

    uint8_t getPacketLoss() const { return packetLoss; }

    bool isPinging() const { return pinging; }
};
