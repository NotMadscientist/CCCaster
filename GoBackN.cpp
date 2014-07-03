#include "GoBackN.h"
#include "Log.h"
#include "Util.h"

#include <string>
#include <cassert>

#define SEND_INTERVAL 100

using namespace std;

string formatSerializableSequence ( const MsgPtr& msg )
{
    assert ( msg->getBaseType() == BaseType::SerializableSequence );
    return toString ( "%u:'%s'", msg->getAs<SerializableSequence>().getSequence(), msg );
}

void GoBackN::timerExpired ( Timer *timer )
{
    assert ( timer == &sendTimer );

    if ( sendList.empty() && !keepAlive )
    {
        return;
    }
    else if ( sendList.empty() && keepAlive )
    {
        owner->sendGoBackN ( this, NullMsg );
    }
    else
    {
        if ( sendListPos == sendList.end() )
            sendListPos = sendList.begin();

        LOG_LIST ( sendList, formatSerializableSequence );

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              *sendListPos, ( **sendListPos ).getAs<SerializableSequence>().getSequence(), sendSequence );
        owner->sendGoBackN ( this, * ( sendListPos++ ) );
    }

    if ( keepAlive )
    {
        LOG ( "this=%08x; keepAlive=%llu; countDown=%llu", this, keepAlive, countDown );

        if ( countDown )
        {
            --countDown;
        }
        else
        {
            LOG ( "owner->timeoutGoBackN ( this=%08x ); owner=%08x", this, owner );
            owner->timeoutGoBackN ( this );
            return;
        }
    }

    sendTimer.start ( SEND_INTERVAL );
}

void GoBackN::send ( SerializableSequence *message )
{
    MsgPtr msg ( message );
    send ( msg );
}

void GoBackN::send ( const MsgPtr& msg )
{
    LOG ( "Adding '%s'; sendSequence=%d", msg, sendSequence + 1 );

    assert ( msg->getBaseType() == BaseType::SerializableSequence );
    assert ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().getSequence() == sendSequence );
    assert ( owner != 0 );

    msg->getAs<SerializableSequence>().setSequence ( ++sendSequence );

    owner->sendGoBackN ( this, msg );

    sendList.push_back ( msg );

    LOG_LIST ( sendList, formatSerializableSequence );

    if ( !sendTimer.isStarted() )
        sendTimer.start ( SEND_INTERVAL );
}

void GoBackN::recv ( const MsgPtr& msg )
{
    assert ( owner != 0 );

    if ( keepAlive )
    {
        // Refresh keep alive count down
        countDown = ( keepAlive / SEND_INTERVAL );

        LOG ( "this=%08x; keepAlive=%llu; countDown=%llu", this, keepAlive, countDown );
    }

    // Ignore non-sequential messages
    if ( !msg.get() || msg->getBaseType() != BaseType::SerializableSequence )
    {
        if ( msg.get() )
            LOG ( "Unexpected '%s'; recvSequence=%u", msg, recvSequence );
        return;
    }

    uint32_t sequence = msg->getAs<SerializableSequence>().getSequence();

    // Check for ACK messages
    if ( msg->getMsgType() == MsgType::AckSequence )
    {
        if ( sequence > ackSequence )
            ackSequence = sequence;

        LOG ( "Got 'AckSequence'; sequence=%u; sendSequence=%u", sequence, sendSequence );

        // Remove messages from sendList with sequence <= the ACKed sequence
        while ( !sendList.empty() && sendList.front()->getAs<SerializableSequence>().getSequence() <= sequence )
            sendList.pop_front();
        sendListPos = sendList.end();

        LOG_LIST ( sendList, formatSerializableSequence );
        return;
    }

    if ( sequence != recvSequence + 1 )
    {
        owner->sendGoBackN ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );
        return;
    }

    LOG ( "Received '%s'; sequence=%u; recvSequence=%u", msg, sequence, recvSequence );

    ++recvSequence;

    owner->sendGoBackN ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );

    if ( keepAlive && !sendTimer.isStarted() )
        sendTimer.start ( SEND_INTERVAL );

    owner->recvGoBackN ( this, msg );
}

void GoBackN::setKeepAlive ( uint64_t timeout )
{
    keepAlive = timeout;
    countDown = ( timeout / SEND_INTERVAL );

    LOG ( "setKeepAlive ( %llu ); countDown=%llu", keepAlive, countDown );
}

void GoBackN::reset()
{
    sendSequence = recvSequence = 0;
    sendList.clear();
    sendListPos = sendList.end();
    sendTimer.stop();
}

GoBackN::GoBackN ( Owner *owner, uint64_t timeout )
    : owner ( owner ), sendSequence ( 0 ), recvSequence ( 0 ), ackSequence ( 0 )
    , sendListPos ( sendList.end() ), sendTimer ( this ), keepAlive ( timeout ), countDown ( timeout / SEND_INTERVAL )
{
}
