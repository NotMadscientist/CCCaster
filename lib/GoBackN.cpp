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
    ASSERT ( timer == _sendTimer.get() );
    ASSERT ( owner != 0 );

    if ( _sendList.empty() && !_keepAlive )
    {
        return;
    }
    else if ( _sendList.empty() && _keepAlive )
    {
        if ( _skipNextKeepAlive )
            _skipNextKeepAlive = false;
        else
            owner->goBackNSendRaw ( this, NullMsg );
    }
    else
    {
        if ( _sendListPos == _sendList.cend() )
            _sendListPos = _sendList.cbegin();

#ifndef DISABLE_LOGGING
        logSendList();
#endif

        const MsgPtr& msg = *_sendListPos;

        LOG ( "Sending '%s'; sequence=%u; sendSequence=%d",
              msg, msg->getAs<SerializableSequence>().getSequence(), _sendSequence );

        owner->goBackNSendRaw ( this, msg );
        ++_sendListPos;
    }

    if ( _keepAlive )
    {
        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, _keepAlive, _countDown );

        if ( _countDown )
        {
            --_countDown;
        }
        else
        {
            LOG ( "owner->goBackNTimeout ( this=%08x ); owner=%08x", this, owner );
            owner->goBackNTimeout ( this );
            return;
        }
    }

    _sendTimer->start ( _interval );
}

void GoBackN::checkAndStartTimer()
{
    if ( ! _sendTimer )
        _sendTimer.reset ( new Timer ( this ) );

    if ( ! _sendTimer->isStarted() )
        _sendTimer->start ( _interval );
}

void GoBackN::sendViaGoBackN ( SerializableSequence *message )
{
    MsgPtr msg ( message );
    sendViaGoBackN ( msg );
}

void GoBackN::sendViaGoBackN ( const MsgPtr& msg )
{
    LOG ( "Adding '%s'; sendSequence=%d", msg, _sendSequence + 1 );

    ASSERT ( msg->getBaseType() == BaseType::SerializableSequence );
    ASSERT ( _sendList.empty() || _sendList.back()->getAs<SerializableSequence>().getSequence() == _sendSequence );
    ASSERT ( owner != 0 );

    if ( msg->getAs<SerializableSequence>().getSequence() != 0 )
    {
        MsgPtr clone = msg->clone();
        clone->getAs<SerializableSequence>().setSequence ( ++_sendSequence );

        owner->goBackNSendRaw ( this, clone );
        _sendList.push_back ( clone );
    }
    else
    {
        msg->getAs<SerializableSequence>().setSequence ( _sendSequence + 1 );
        string bytes = ::Protocol::encode ( msg );

        if ( bytes.size() <= MTU )
        {
            ++_sendSequence;
            owner->goBackNSendRaw ( this, msg );
            _sendList.push_back ( msg );
        }
        else
        {
            const uint32_t count = ( bytes.size() / MTU ) + ( bytes.size() % MTU == 0 ? 0 : 1 );

            for ( uint32_t pos = 0, i = 0; pos < bytes.size(); pos += MTU, ++i )
            {
                SplitMessage *splitMsg = new SplitMessage ( msg->getMsgType(), bytes.substr ( pos, MTU ), i, count );
                splitMsg->setSequence ( ++_sendSequence );

                MsgPtr msg ( splitMsg );
                owner->goBackNSendRaw ( this, msg );
                _sendList.push_back ( msg );
            }
        }
    }

    logSendList();

    checkAndStartTimer();
}

void GoBackN::recvFromSocket ( const MsgPtr& msg )
{
    ASSERT ( owner != 0 );

    if ( _keepAlive )
    {
        refreshKeepAlive();

        LOG ( "this=%08x; keepAlive=%llu; countDown=%d", this, _keepAlive, _countDown );

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

    const uint32_t sequence = msg->getAs<SerializableSequence>().getSequence();

    // Check for ACK messages
    if ( msg->getMsgType() == MsgType::AckSequence )
    {
        if ( sequence > _ackSequence )
            _ackSequence = sequence;

        LOG ( "Got AckSequence; sequence=%u; sendSequence=%u", sequence, _sendSequence );

        // Remove messages from sendList with sequence <= the ACKed sequence
        while ( !_sendList.empty() && _sendList.front()->getAs<SerializableSequence>().getSequence() <= sequence )
            _sendList.pop_front();
        _sendListPos = _sendList.cend();

        logSendList();
        return;
    }

    if ( sequence != _recvSequence + 1 )
    {
        owner->goBackNSendRaw ( this, MsgPtr ( new AckSequence ( _recvSequence ) ) );
        return;
    }

    LOG ( "Received '%s'; sequence=%u; recvSequence=%u", msg, sequence, _recvSequence );

    ++_recvSequence;

    owner->goBackNSendRaw ( this, MsgPtr ( new AckSequence ( _recvSequence ) ) );

    if ( msg->getMsgType() == MsgType::SplitMessage )
    {
        const SplitMessage& splitMsg = msg->getAs<SplitMessage>();

        _recvBuffer += splitMsg.bytes;

        if ( splitMsg.isLastMessage() )
        {
            size_t consumed = 0;
            MsgPtr msg = ::Protocol::decode ( &_recvBuffer[0], _recvBuffer.size(), consumed );

            if ( !msg.get() || msg->getMsgType() != splitMsg.origMsgType || consumed != _recvBuffer.size() )
            {
                LOG ( "Failed to recreate '%s' from [ %u bytes ]", splitMsg.origMsgType, _recvBuffer.size() );
                msg.reset();
            }

            _recvBuffer.clear();

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

    _interval = interval;

    refreshKeepAlive();

    LOG ( "interval=%llu; countDown=%d", _interval, _countDown );
}

void GoBackN::setKeepAlive ( uint64_t timeout )
{
    _keepAlive = timeout;

    refreshKeepAlive();

    LOG ( "keepAlive=%llu; countDown=%d", _keepAlive, _countDown );
}

void GoBackN::reset()
{
    LOG ( "this=%08x; sendTimer=%08x", this, _sendTimer.get() );

    _sendSequence = _recvSequence = 0;
    _sendList.clear();
    _sendListPos = _sendList.cend();
    _sendTimer.reset();
    _recvBuffer.clear();
}

GoBackN::GoBackN ( Owner *owner, uint64_t interval, uint64_t timeout )
    : owner ( owner )
    , _sendListPos ( _sendList.cend() )
    , _interval ( interval )
    , _keepAlive ( timeout )
{
    ASSERT ( _interval > 0 );

    refreshKeepAlive();
}

GoBackN::GoBackN ( Owner *owner, const GoBackN& state )
    : owner ( owner )
    , _sendListPos ( _sendList.cend() )
{
    *this = state;
}

GoBackN::GoBackN ( const GoBackN& other )
{
    *this = other;
}

GoBackN& GoBackN::operator= ( const GoBackN& other )
{
    _sendSequence = other._sendSequence;
    _recvSequence = other._recvSequence;
    _ackSequence = other._ackSequence;
    _sendList = other._sendList;
    _interval = other._interval;
    _keepAlive = other._keepAlive;
    _countDown = other._keepAlive;

    ASSERT ( _interval > 0 );

    return *this;
}

void GoBackN::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( _recvBuffer, _keepAlive, _sendSequence, _recvSequence, _ackSequence );

    ar ( _sendList.size() );

    for ( const MsgPtr& msg : _sendList )
        ar ( Protocol::encode ( msg ) );
}

void GoBackN::load ( cereal::BinaryInputArchive& ar )
{
    ar ( _recvBuffer, _keepAlive, _sendSequence, _recvSequence, _ackSequence );

    size_t size, consumed;
    ar ( size );

    string buffer;
    for ( size_t i = 0; i < size; ++i )
    {
        ar ( buffer );
        _sendList.push_back ( Protocol::decode ( &buffer[0], buffer.size(), consumed ) );
    }
}

void GoBackN::logSendList() const
{
    LOG_LIST ( _sendList, formatSerializableSequence );
}

void GoBackN::delayKeepAliveOnce()
{
    _skipNextKeepAlive = true;
}

void GoBackN::refreshKeepAlive()
{
    _countDown = ( _keepAlive / _interval );
}
