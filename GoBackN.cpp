#include "GoBackN.h"
#include "Log.h"
#include "Util.h"

#include <string>
#include <cassert>

#define SEND_INTERVAL 100

using namespace std;

#define LOG_LIST(LIST)                                                                                                \
    do {                                                                                                              \
        if ( !Log::isEnabled )                                                                                        \
            break;                                                                                                    \
        string list;                                                                                                  \
        for ( const MsgPtr& msg : LIST )                                                                              \
            list += toString ( " %u:'", msg->getAs<SerializableSequence>().getSequence() ) + toString ( msg ) + "',"; \
        if ( !LIST.empty() )                                                                                          \
            list [ list.size() - 1 ] = ' ';                                                                           \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list.c_str() );                                                       \
    } while ( 0 )

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

        LOG_LIST ( sendList );

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              TO_C_STR ( *sendListPos ), ( **sendListPos ).getAs<SerializableSequence>().getSequence(), sendSequence );
        owner->sendGoBackN ( this, *sendListPos );
        ++sendListPos;
    }

    if ( keepAlive )
    {
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
    LOG ( "Adding '%s'; sendSequence=%d", TO_C_STR ( msg ), sendSequence + 1 );

    assert ( msg->getBaseType() == BaseType::SerializableSequence );
    assert ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().getSequence() == sendSequence );
    assert ( owner != 0 );

    msg->getAs<SerializableSequence>().setSequence ( ++sendSequence );

    owner->sendGoBackN ( this, msg );

    sendList.push_back ( msg );

    LOG_LIST ( sendList );

    if ( !sendTimer.isStarted() )
        sendTimer.start ( SEND_INTERVAL );
}

void GoBackN::recv ( const MsgPtr& msg )
{
    assert ( owner != 0 );

    // Refresh keep alive count down
    countDown = ( keepAlive / SEND_INTERVAL );

    // Ignore non-sequential messages
    if ( !msg.get() || msg->getBaseType() != BaseType::SerializableSequence )
    {
        if ( msg.get() )
            LOG ( "Unexpected '%s'; recvSequence=%u", TO_C_STR ( msg ), recvSequence );
        return;
    }

    uint32_t sequence = msg->getAs<SerializableSequence>().getSequence();

    // Check for ACK messages
    if ( msg->getType() == MsgType::AckSequence )
    {
        if ( sequence > ackSequence )
            ackSequence = sequence;

        LOG ( "Got 'AckSequence'; sequence=%u; sendSequence=%u", sequence, sendSequence );

        // Remove messages from sendList with sequence <= the ACKed sequence
        while ( !sendList.empty() && sendList.front()->getAs<SerializableSequence>().getSequence() <= sequence )
            sendList.pop_front();
        sendListPos = sendList.end();

        LOG_LIST ( sendList );
        return;
    }

    if ( sequence != recvSequence + 1 )
    {
        owner->sendGoBackN ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );
        return;
    }

    LOG ( "Received '%s'; sequence=%u; recvSequence=%u", TO_C_STR ( msg ), sequence, recvSequence );

    ++recvSequence;

    owner->recvGoBackN ( this, msg );

    if ( recvSequence )
    {
        owner->sendGoBackN ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );

        if ( keepAlive && !sendTimer.isStarted() )
            sendTimer.start ( SEND_INTERVAL );
    }
}

void GoBackN::setKeepAlive ( const uint64_t& timeout )
{
    keepAlive = timeout;
    countDown = ( timeout / SEND_INTERVAL );
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
    , sendListPos ( sendList.end() ), sendTimer ( this ), keepAlive ( timeout ), countDown ( 0 )
{
}
