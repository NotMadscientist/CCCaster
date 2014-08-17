#include "Pinger.h"
#include "Logger.h"
#include "TimerManager.h"


#define MAX_ROUND_TRIP 500


Pinger::Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings )
    : owner ( owner ), pingInterval ( pingInterval ), numPings ( numPings ) {}

void Pinger::start()
{
    LOG ( "Start pinging" );

    TimerManager::get().updateNow();

    if ( owner )
        owner->sendPing ( this, MsgPtr ( new Ping ( TimerManager::get().getNow() ) ) );

    pingCount = 1;

    pingTimer.reset ( new Timer ( this ) );
    pingTimer->start ( pingInterval );

    pinging = true;
}

void Pinger::gotPong ( const MsgPtr& ping )
{
    ASSERT ( ping.get() != 0 );
    ASSERT ( ping->getMsgType() == MsgType::Ping );

    uint64_t now = TimerManager::get().getNow();

    if ( now < ping->getAs<Ping>().timestamp )
        return;

    if ( pinging )
    {
        uint64_t latency = ( now - ping->getAs<Ping>().timestamp ) / 2;

        LOG ( "latency=%llu ms", latency );

        stats.addValue ( latency );
    }
    else
    {
        if ( owner )
            owner->sendPing ( this, ping );
    }
}

void Pinger::timerExpired ( Timer *timer )
{
    ASSERT ( timer == pingTimer.get() );

    if ( pingCount >= numPings )
    {
        LOG ( "Done pinging" );

        if ( owner )
            owner->donePinging ( this, stats, 100 * ( numPings - stats.getNumSamples() ) / numPings );

        pingTimer.reset();
        pinging = false;
        return;
    }

    if ( owner )
        owner->sendPing ( this, MsgPtr ( new Ping ( TimerManager::get().getNow() ) ) );

    ++pingCount;

    pingTimer->start ( pingCount < numPings ? pingInterval : MAX_ROUND_TRIP );
}
