#pragma once

#include "Timer.hpp"
#include "Protocol.hpp"
#include "Statistics.hpp"


struct Ping : public SerializableMessage
{
    uint64_t timestamp;

    Ping ( uint64_t timestamp ) : timestamp ( timestamp ) {}

    std::string str() const override { return format ( "Ping[%llu]", timestamp ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( Ping, timestamp )
};


class Pinger : private Timer::Owner
{
public:

    struct Owner
    {
        virtual void pingerSendPing ( Pinger *pinger, const MsgPtr& ping ) = 0;

        virtual void pingerCompleted ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) = 0;
    };

    Owner *owner = 0;

    uint64_t pingInterval = 0;

    size_t numPings = 0;

    Pinger();

    Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings );

    void start();

    void stop();

    void reset();

    void gotPong ( const MsgPtr& ping );

    const Statistics& getStats() const { return _stats; }

    uint8_t getPacketLoss() const { return _packetLoss; }

    bool isPinging() const { return _pinging; }

private:

    TimerPtr _pingTimer;

    size_t _pingCount = 0;

    Statistics _stats;

    uint8_t _packetLoss = 0;

    bool _pinging = false;

    void timerExpired ( Timer *timer ) override;
};
