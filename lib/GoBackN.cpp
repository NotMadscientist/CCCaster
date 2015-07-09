#include "GoBackN.hpp"
#include "Logger.hpp"

#include <cereal/types/string.hpp>

#include <string>

using namespace std;


// TODO increase me
#define MTU ( 256 )


string formatSerializableSequence ( const MsgPtr& msg )
{
    ASSERT ( msg->getBaseType() == BaseType::SerializableSequence );
    return format ( "%u:'%s'", msg->getAs<SerializableSequence>().getSequence(), msg );
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
            owner->goBackNSendRaw ( this, NullMsg );
    }
    else
    {
        if ( sendListPos == sendList.cend() )
            sendListPos = sendList.cbegin();

#ifndef DISABLE_LOGGING
        logSendList();
#endif

        const MsgPtr& msg = *sendListPos;

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              msg, msg->getAs<SerializableSequence>().getSequence(), sendSequence );

        owner->goBackNSendRaw ( this, msg );
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
            LOG ( "owner->goBackNTimeout ( this=%08x ); owner=%08x", this, owner );
            owner->goBackNTimeout ( this );
            return;
        }
    }

    sendTimer->start ( interval );
}

void GoBackN::checkAndStartTimer()
{
    if ( ! sendTimer )
        sendTimer.reset ( new Timer ( this ) );

    if ( ! sendTimer->isStarted() )
        sendTimer->start ( interval );
}

void GoBackN::sendViaGoBackN ( SerializableSequence *message )
{
    MsgPtr msg ( message );
    sendViaGoBackN ( msg );
}

void GoBackN::sendViaGoBackN ( const MsgPtr& msg )
{
    LOG ( "Adding '%s'; sendSequence=%d", msg, sendSequence + 1 );

    ASSERT ( msg->getBaseType() == BaseType::SerializableSequence );
    ASSERT ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().getSequence() == sendSequence );
    ASSERT ( owner != 0 );

    if ( msg->getAs<SerializableSequence>().getSequence() != 0 )
    {
        MsgPtr clone = msg->clone();
        clone->getAs<SerializableSequence>().setSequence ( ++sendSequence );

        owner->goBackNSendRaw ( this, clone );
        sendList.push_back ( clone );
    }
    else
    {
        msg->getAs<SerializableSequence>().setSequence ( sendSequence + 1 );
        string bytes = ::Protocol::encode ( msg );

        if ( bytes.size() <= MTU )
        {
            ++sendSequence;
            owner->goBackNSendRaw ( this, msg );
            sendList.push_back ( msg );
        }
        else
        {
            const uint32_t count = ( bytes.size() / MTU ) + ( bytes.size() % MTU == 0 ? 0 : 1 );

            for ( uint32_t pos = 0, i = 0; pos < bytes.size(); pos += MTU, ++i )
            {
                SplitMessage *splitMsg = new SplitMessage ( msg->getMsgType(), bytes.substr ( pos, MTU ), i, count );
                splitMsg->setSequence ( ++sendSequence );

                MsgPtr msg ( splitMsg );
                owner->goBackNSendRaw ( this, msg );
                sendList.push_back ( msg );
            }
        }
    }

    logSendList();

    checkAndStartTimer();
}

void GoBackN::recvFromSocket ( const MsgPtr& msg )
{
    ASSERT ( owner != 0 );

    if ( keepAlive )
    {
        refreshKeepAlive();

        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, keepAlive, countDown );

        checkAndStartTimer();
    }

    // Ignore null keep alive messages
    if ( ! msg.get() )
        return;

    // Filter non-sequential messages
    if ( msg->getBaseType() != BaseType::SerializableSequence )
    {
        LOG ( "Received '%s'", msg );
        owner->goBackNRecvRaw ( this, msg );
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
        sendListPos = sendList.cend();

        logSendList();
        return;
    }

    if ( sequence != recvSequence + 1 )
    {
        owner->goBackNSendRaw ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );
        return;
    }

    LOG ( "Received '%s'; sequence=%u; recvSequence=%u", msg, sequence, recvSequence );

    ++recvSequence;

    owner->goBackNSendRaw ( this, MsgPtr ( new AckSequence ( recvSequence ) ) );

    if ( msg->getMsgType() == MsgType::SplitMessage )
    {
        const SplitMessage& splitMsg = msg->getAs<SplitMessage>();

        recvBuffer += splitMsg.bytes;

        if ( splitMsg.isLastMessage() )
        {
            size_t consumed = 0;
            MsgPtr msg = ::Protocol::decode ( &recvBuffer[0], recvBuffer.size(), consumed );

            if ( !msg.get() || msg->getMsgType() != splitMsg.origMsgType || consumed != recvBuffer.size() )
            {
                LOG ( "Failed to recreate '%s' from [ %u bytes ]", splitMsg.origMsgType, recvBuffer.size() );
                msg.reset();
            }

            recvBuffer.clear();

            if ( msg )
            {
                LOG ( "Recreated '%s'", msg );
                owner->goBackNRecvMsg ( this, msg );
            }
        }

        return;
    }

    owner->goBackNRecvMsg ( this, msg );
}

void GoBackN::setSendInterval ( uint64_t interval )
{
    ASSERT ( interval > 0 );

    this->interval = interval;

    refreshKeepAlive();

    LOG ( "interval=%llu; countDown=%d", interval, countDown );
}

void GoBackN::setKeepAlive ( uint64_t timeout )
{
    keepAlive = timeout;

    refreshKeepAlive();

    LOG ( "keepAlive=%llu; countDown=%d", keepAlive, countDown );
}

void GoBackN::reset()
{
    LOG ( "this=%08x; sendTimer=%08x", this, sendTimer.get() );

    sendSequence = recvSequence = 0;
    sendList.clear();
    sendListPos = sendList.cend();
    sendTimer.reset();
    recvBuffer.clear();
}

GoBackN::GoBackN ( Owner *owner, uint64_t interval, uint64_t timeout )
    : owner ( owner )
    , sendListPos ( sendList.cend() )
    , interval ( interval )
    , keepAlive ( timeout )
{
    ASSERT ( interval > 0 );

    refreshKeepAlive();
}

GoBackN::GoBackN ( Owner *owner, const GoBackN& state ) : owner ( owner ), sendListPos ( sendList.cend() )
{
    *this = state;
}

GoBackN& GoBackN::operator= ( const GoBackN& other )
{
    sendSequence = other.sendSequence;
    recvSequence = other.recvSequence;
    ackSequence = other.ackSequence;
    sendList = other.sendList;
    interval = other.interval;
    keepAlive = other.keepAlive;
    countDown = other.keepAlive;

    ASSERT ( interval > 0 );

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
