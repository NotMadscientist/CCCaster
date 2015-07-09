#include "Pinger.hpp"
#include "Logger.hpp"
#include "TimerManager.hpp"


#define MAX_ROUND_TRIP 500


Pinger::Pinger() {}

Pinger::Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings )
    : owner ( owner ), pingInterval ( pingInterval ), numPings ( numPings )
{
    ASSERT ( pingInterval > 0 );
    ASSERT ( numPings > 0 );
}

void Pinger::start()
{
    LOG ( "Start pinging" );

    ASSERT ( pingInterval > 0 );
    ASSERT ( numPings > 0 );

    if ( owner )
        owner->pingerSendPing ( this, MsgPtr ( new Ping ( TimerManager::get().getNow ( true ) ) ) );

    pingCount = 1;

    ASSERT ( pingInterval > 0 );

    pingTimer.reset ( new Timer ( this ) );
    pingTimer->start ( pingInterval );

    pinging = true;
}

void Pinger::stop()
{
    LOG ( "Stop pinging" );

    pingTimer.reset();
    pingCount = 0;
    pinging = false;
}

void Pinger::reset()
{
    stop();

    stats.reset();
    packetLoss = 0;
}

void Pinger::gotPong ( const MsgPtr& ping )
{
    ASSERT ( ping.get() != 0 );
    ASSERT ( ping->getMsgType() == MsgType::Ping );

    if ( pinging )
    {
        const uint64_t now = TimerManager::get().getNow();

        if ( now < ping->getAs<Ping>().timestamp )
            return;

        const uint64_t latency = ( now - ping->getAs<Ping>().timestamp ) / 2;

        LOG ( "latency=%llu ms", latency );

        stats.addSample ( latency );
    }
    else
    {
        if ( owner )
            owner->pingerSendPing ( this, ping );
    }
}

void Pinger::timerExpired ( Timer *timer )
{
    ASSERT ( timer == pingTimer.get() );

    if ( pingCount >= numPings )
    {
        LOG ( "Done pinging" );

        packetLoss = 100 * ( numPings - stats.getNumSamples() ) / numPings;

        if ( owner )
            owner->pingerCompleted ( this, stats, packetLoss );

        stop();
        return;
    }

    if ( owner )
        owner->pingerSendPing ( this, MsgPtr ( new Ping ( TimerManager::get().getNow() ) ) );

    ++pingCount;

    ASSERT ( pingInterval > 0 );

    pingTimer->start ( pingCount < numPings ? pingInterval : MAX_ROUND_TRIP );
}
