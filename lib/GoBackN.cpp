#include "GoBackN.h"
#include "Logger.h"
#include "Utilities.h"

#include <cereal/types/string.hpp>

#include <string>

using namespace std;


#define SEND_INTERVAL ( 50 )

#define MTU ( 256 )


string formatSerializableSequence ( const MsgPtr& msg )
{
    ASSERT ( msg->getBaseType() == BaseType::SerializableSequence );
    return toString ( "%u:'%s'", msg->getAs<SerializableSequence>().getSequence(), msg );
}


void GoBackN::timerExpired ( Timer *timer )
{
    ASSERT ( timer == sendTimer.get() );
    ASSERT ( owner != 0 );

    if ( sendList.empty() && !keepAlive )
    {
        return;
    }
    else if ( sendList.empty() && keepAlive )
    {
        if ( skipNextKeepAlive )
            skipNextKeepAlive = false;
        else
            owner->sendRaw ( this, NullMsg );
    }
    else
    {
        if ( sendListPos == sendList.end() )
            sendListPos = sendList.begin();

        logSendList();

        const MsgPtr& msg = *sendListPos;

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              msg, msg->getAs<SerializableSequence>().getSequence(), sendSequence );

        owner->sendRaw ( this, msg );
        ++sendListPos;
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

void GoBackN::checkAndStartTimer()
{
    if ( !sendTimer )
        sendTimer.reset ( new Timer ( this ) );

    if ( !sendTimer->isStarted() )
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

    ASSERT ( msg->getBaseType() == BaseType::SerializableSequence );
    ASSERT ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().getSequence() == sendSequence );
    ASSERT ( owner != 0 );

    msg->getAs<SerializableSequence>().setSequence ( sendSequence + 1 );
    string bytes = ::Protocol::encode ( msg );

    if ( bytes.size() <= MTU )
    {
        ++sendSequence;
        owner->sendRaw ( this, msg );
        sendList.push_back ( msg );
    }
    else
    {
        const uint32_t count = ( bytes.size() / MTU ) + ( bytes.size() % MTU == 0 ? 0 : 1 );

        for ( uint32_t pos = 0, i = 0; pos < bytes.size(); pos += MTU, ++i )
        {
            SplitMessage *splitMessage = new SplitMessage ( msg->getMsgType(), bytes.substr ( pos, MTU ), i, count );
            splitMessage->setSequence ( ++sendSequence );

            MsgPtr msg ( splitMessage );
            owner->sendRaw ( this, msg );
            sendList.push_back ( msg );
        }
    }

    logSendList();

    checkAndStartTimer();
}

void GoBackN::recvRaw ( const MsgPtr& msg )
{
    ASSERT ( owner != 0 );

    if ( keepAlive )
    {
        // Refresh keep alive count down
        countDown = ( keepAlive / SEND_INTERVAL );

        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, keepAlive, countDown );

        checkAndStartTimer();
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

        logSendList();
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

    if ( msg->getMsgType() == MsgType::SplitMessage )
    {
        const SplitMessage& splitMessage = msg->getAs<SplitMessage>();

        recvBuffer += splitMessage.bytes;

        if ( splitMessage.isLastMessage() )
        {
            size_t consumed = 0;
            MsgPtr msg = ::Protocol::decode ( &recvBuffer[0], recvBuffer.size(), consumed );

            if ( !msg || msg->getMsgType() != splitMessage.origMsgType || consumed != recvBuffer.size() )
            {
                LOG ( "Failed to recreate '%s' from [ %u bytes ]", splitMessage.origMsgType, recvBuffer.size() );
                msg.reset();
            }

            recvBuffer.clear();

            if ( msg )
            {
                LOG ( "Recreated '%s'", msg );
                owner->recvGoBackN ( this, msg );
            }
        }

        return;
    }

    owner->recvGoBackN ( this, msg );
}

void GoBackN::setKeepAlive ( uint64_t timeout )
{
    keepAlive = timeout;
    countDown = ( timeout / SEND_INTERVAL );

    LOG ( "keepAlive=%llu; countDown=%d", keepAlive, countDown );
}

void GoBackN::reset()
{
    LOG ( "this=%08x; sendTimer=%08x", this, sendTimer.get() );

    sendSequence = recvSequence = 0;
    sendList.clear();
    sendListPos = sendList.end();
    sendTimer.reset();
    recvBuffer.clear();
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
    ar ( recvBuffer, keepAlive, sendSequence, recvSequence, ackSequence );

    ar ( sendList.size() );

    for ( const MsgPtr& msg : sendList )
        ar ( Protocol::encode ( msg ) );
}

void GoBackN::load ( cereal::BinaryInputArchive& ar )
{
    ar ( recvBuffer, keepAlive, sendSequence, recvSequence, ackSequence );

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
