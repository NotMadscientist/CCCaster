#include "GoBackN.h"
#include "Log.h"
#include "Util.h"

#include <string>
#include <cassert>

#define SEND_INTERVAL       100

using namespace std;

void GoBackN::timerExpired ( Timer *timer )
{
    assert ( timer == &sendTimer );

    // TODO
}

void GoBackN::send ( const MsgPtr& msg )
{
    LOG ( "Added '%s' to resend", TO_C_STR ( msg ) );

    assert ( msg->base() == BaseType::SerializableSequence );
    assert ( sendList.empty() || sendList.back()->getAs<SerializableSequence>().sequence == sendSequence );
    assert ( owner != 0 );

    msg->getAs<SerializableSequence>().sequence = sendSequence++;

    owner->send ( msg );

    sendList.push_back ( msg );

    if ( Log::isEnabled )
    {
        string list;
        for ( const MsgPtr& msg : sendList )
            list += " '" + toString ( msg ) + "',";
        LOG ( "sendList=%s", list.c_str() );
    }

    if ( !sendTimer.isStarted() )
        sendTimer.start ( SEND_INTERVAL );
}

void GoBackN::recv ( const MsgPtr& msg )
{
    assert ( owner != 0 );

    if ( !msg.get() || msg->base() != BaseType::SerializableSequence )
    {
        LOG ( "Unexpected '%s'", TO_C_STR ( msg ) );
        return;
    }

    uint32_t sequence = msg->getAs<SerializableSequence>().sequence;

    if ( msg->type() == MsgType::AckSequence )
    {
        LOG ( "Got 'AckSequence'; sequence=%u; sendSequence=%u", sequence, sendSequence );

        while ( !sendList.empty() || sendList.front()->getAs<SerializableSequence>().sequence <= sequence )
            sendList.pop_front();

        if ( Log::isEnabled )
        {
            string list;
            for ( const MsgPtr& msg : sendList )
                list += " '" + toString ( msg ) + "',";
            LOG ( "sendList=%s", list.c_str() );
        }

        return;
    }

    LOG ( "Got '%s'; sequence=%u; recvSequence=%u", TO_C_STR ( msg ), sequence, recvSequence );

    if ( recvList.empty() || sequence < recvList.front()->getAs<SerializableSequence>().sequence )
    {
        recvList.push_front ( msg );
    }
    else
    {
        auto it = recvList.begin();
        for ( ++it; it != recvList.end(); ++it )
            if ( sequence < ( **it ).getAs<SerializableSequence>().sequence )
                break;
        recvList.insert ( it, msg );
    }

    if ( Log::isEnabled )
    {
        string list;
        for ( const MsgPtr& msg : recvList )
            list += " '" + toString ( msg ) + "',";
        LOG ( "recvList=%s", list.c_str() );
    }

    while ( recvList.front()->getAs<SerializableSequence>().sequence == recvSequence )
    {
        LOG ( "Received '%s'; sequence=%u", TO_C_STR ( recvList.front() ), recvSequence );
        owner->recv ( recvList.front() );

        recvList.pop_front();
        ++recvSequence;

        if ( recvList.empty() )
            break;
    }
}

void GoBackN::reset()
{
    // TODO
}

GoBackN::GoBackN ( Owner *owner )
    : owner ( owner ), sendSequence ( 0 ), recvSequence ( 0 ), sendListPos ( sendList.end() ), sendTimer ( this )
{
}
