#include "GoBackN.h"
#include "Logger.h"
#include "Utilities.h"

#include <cereal/types/string.hpp>

#include <string>
#include <cassert>

using namespace std;


#define SEND_INTERVAL 100


string formatSerializableSequence ( const MsgPtr& msg )
{
    assert ( msg->getBaseType() == BaseType::SerializableSequence );
    return toString ( "%u:'%s'", msg->getAs<SerializableSequence>().getSequence(), msg );
}


void GoBackN::timerExpired ( Timer *timer )
{
    assert ( timer == sendTimer.get() );

    if ( sendList.empty() && !keepAlive )
    {
        return;
    }
    else if ( sendList.empty() && keepAlive )
    {
        owner->sendRaw ( this, NullMsg );
    }
    else
    {
        if ( sendListPos == sendList.end() )
            sendListPos = sendList.begin();

        LOG_LIST ( sendList, formatSerializableSequence );

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              *sendListPos, ( **sendListPos ).getAs<SerializableSequence>().getSequence(), sendSequence );
        owner->sendRaw ( this, * ( sendListPos++ ) );
    }

    if ( keepAlive )
    {
        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, keepAlive, countDown );

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

    sendTimer->start ( SEND_INTERVAL );
}

void GoBackN::sendGoBackN ( SerializableSequence *message )
{
    MsgPtr msg ( message );
    sendGoBackN ( msg );
}

void GoBackN::sendGoBackN ( const MsgPtr& msg )
{
    LOG ( "Adding '%s'; sendSequence=%d", msg, sendSequence + 1 );

    assert ( msg->getBaseType() == BaseType::SerializableSequence );
    assert ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().getSequence() == sendSequence );
    assert ( owner != 0 );

    msg->getAs<SerializableSequence>().setSequence ( ++sendSequence );

    owner->sendRaw ( this, msg );

    sendList.push_back ( msg );

    LOG_LIST ( sendList, formatSerializableSequence );

    if ( !sendTimer )
        sendTimer.reset ( new Timer ( this ) );

    if ( !sendTimer->isStarted() )
        sendTimer->start ( SEND_INTERVAL );
}

void GoBackN::recvRaw ( const MsgPtr& msg )
{
    assert ( owner != 0 );

    if ( keepAlive )
    {
        // Refresh keep alive count down
        countDown = ( keepAlive / SEND_INTERVAL );

        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, keepAlive, countDown );
    }

    // Ignore null keep alive messages
    if ( !msg )
        return;

    // Filter non-sequential messages
    if ( msg->getBaseType() != BaseType::SerializableSequence )
    {
        LOG ( "Received '%s'", msg );
        owner->recvRaw ( this, msg );
        return;
    }

    uint32_t sequence = msg->getAs<SerializableSequence>().getSequence();

    // Check for ACK messages
    if ( msg->getMsgType() == MsgType::AckSequence )
    {
        if ( sequence > ackSequence )
            ackSequence = sequence;

        LOG ( "Got AckSequence; sequence=%u; sendSequence=%u", sequence, sendSequence );

        // Remove messages from sendList with sequence <= the ACKed sequence
        while ( !sendList.empty() && sendList.front()->getAs<SerializableSequence>().getSequence() <= sequence )
            sendList.pop_front();
        sendListPos = sendList.end();

        LOG_LIST ( sendList, formatSerializableSequence );
        return;
    }

    if ( sequence != recvSequence + 1 )
    {
        owner->sendRaw ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );
        return;
    }

    LOG ( "Received '%s'; sequence=%u; recvSequence=%u", msg, sequence, recvSequence );

    ++recvSequence;

    owner->sendRaw ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );

    if ( keepAlive )
    {
        if ( !sendTimer )
            sendTimer.reset ( new Timer ( this ) );

        if ( !sendTimer->isStarted() )
            sendTimer->start ( SEND_INTERVAL );
    }

    owner->recvGoBackN ( this, msg );
}

void GoBackN::setKeepAlive ( uint64_t timeout )
{
    keepAlive = timeout;
    countDown = ( timeout / SEND_INTERVAL );

    LOG ( "setKeepAlive ( %llu ); countDown=%d", keepAlive, countDown );
}

void GoBackN::reset()
{
    sendSequence = recvSequence = 0;
    sendList.clear();
    sendListPos = sendList.end();
    sendTimer.reset();

    LOG ( "reset GoBackN state" );
}

GoBackN::GoBackN ( Owner *owner, uint64_t timeout )
    : owner ( owner )
    , sendListPos ( sendList.end() )
    , keepAlive ( timeout )
    , countDown ( timeout / SEND_INTERVAL ) {}

GoBackN::GoBackN ( Owner *owner, const GoBackN& state ) : owner ( owner ), sendListPos ( sendList.end() )
{
    *this = state;
}

GoBackN& GoBackN::operator= ( const GoBackN& other )
{
    sendSequence = other.sendSequence;
    recvSequence = other.recvSequence;
    ackSequence = other.ackSequence;
    sendList = other.sendList;
    keepAlive = other.keepAlive;
    countDown = other.keepAlive;

    return *this;
}

void GoBackN::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( keepAlive, sendSequence, recvSequence, ackSequence );

    ar ( sendList.size() );

    for ( const MsgPtr& msg : sendList )
        ar ( Protocol::encode ( msg ) );
}

void GoBackN::load ( cereal::BinaryInputArchive& ar )
{
    ar ( keepAlive, sendSequence, recvSequence, ackSequence );

    size_t size, consumed;
    ar ( size );

    string buffer;
    for ( size_t i = 0; i < size; ++i )
    {
        ar ( buffer );
        sendList.push_back ( Protocol::decode ( &buffer[0], buffer.size(), consumed ) );
    }
}

void GoBackN::logSendList() const
{
    LOG_LIST ( sendList, formatSerializableSequence );
}
