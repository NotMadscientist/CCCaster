#include "Pinger.hpp"
#include "Logger.hpp"
#include "TimerManager.hpp"


#define MAX_ROUND_TRIP 500


Pinger::Pinger() {}

Pinger::Pinger ( Owner *owner, uint64_t pingInterval, size_t numPings )
    : owner ( owner )
    , pingInterval ( pingInterval )
    , numPings ( numPings )
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

    _pingCount = 1;

    ASSERT ( pingInterval > 0 );

    _pingTimer.reset ( new Timer ( this ) );
    _pingTimer->start ( pingInterval );

    _pinging = true;
}

void Pinger::stop()
{
    LOG ( "Stop pinging" );

    _pingTimer.reset();
    _pingCount = 0;
    _pinging = false;
}

void Pinger::reset()
{
    stop();

    _stats.reset();
    _packetLoss = 0;
}

void Pinger::gotPong ( const MsgPtr& ping )
{
    ASSERT ( ping.get() != 0 );
    ASSERT ( ping->getMsgType() == MsgType::Ping );

    if ( _pinging )
    {
        const uint64_t now = TimerManager::get().getNow();

        if ( now < ping->getAs<Ping>().timestamp )
            return;

        const uint64_t latency = ( now - ping->getAs<Ping>().timestamp ) / 2;

        LOG ( "latency=%llu ms", latency );

        _stats.addSample ( latency );
    }
    else
    {
        if ( owner )
            owner->pingerSendPing ( this, ping );
    }
}

void Pinger::timerExpired ( Timer *timer )
{
    ASSERT ( timer == _pingTimer.get() );

    if ( _pingCount >= numPings )
    {
        LOG ( "Done pinging" );

        _packetLoss = 100 * ( numPings - _stats.getNumSamples() ) / numPings;

        if ( owner )
            owner->pingerCompleted ( this, _stats, _packetLoss );

        stop();
        return;
    }

    if ( owner )
        owner->pingerSendPing ( this, MsgPtr ( new Ping ( TimerManager::get().getNow() ) ) );

    ++_pingCount;

    ASSERT ( pingInterval > 0 );

    _pingTimer->start ( _pingCount < numPings ? pingInterval : MAX_ROUND_TRIP );
}
