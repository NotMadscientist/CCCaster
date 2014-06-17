#include "DoubleSocket.h"
#include "Log.h"
#include "Util.h"

#include <cassert>

#define RESEND_INTERVAL     100

using namespace std;

#define LOG_UNEXPECTED_READ                                                                                     \
    LOG ( "Unexpected '%s' from %s socket %08x address '%s' while '%s'",                                        \
          TO_C_STR ( msg ), TO_C_STR ( socket->getProtocol() ), socket, address.c_str(), TO_C_STR ( state ) )

void DoubleSocket::acceptEvent ( Socket *serverSocket )
{
    assert ( serverSocket == primary.get() );

    // shared_ptr<Socket> socket ( serverSocket->accept ( *this ) );

    // assert ( socket.get() );

    // if ( state != State::Listening )
    // return;

    // socket->send ( PrimaryId ( 0, ( uint32_t ) socket.get() ) );

    Socket *socket = serverSocket->accept ( *this );

    if ( state != State::Listening )
        return;

    socket->send ( PrimaryId ( 0, ( uint32_t ) socket ) );
}

void DoubleSocket::connectEvent ( Socket *socket )
{
    // assert ( socket == primary.get() );
}

void DoubleSocket::disconnectEvent ( Socket *socket )
{
    state = State::Disconnected;
}

void DoubleSocket::readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
{
    MsgPtr msg;

    if ( len > 0 )
    {
        msg = Serializable::decode ( bytes, len );

        if ( !msg.get() )
        {
            LOG ( "Failed to decode [ %u bytes ]", len );
            return;
        }
    }

    switch ( state )
    {
        default:
            LOG_UNEXPECTED_READ;
            break;

        case State::Listening:
            if ( socket != secondary.get() )
            {
                LOG_UNEXPECTED_READ;
                break;
            }

            if ( msg->type() != MsgType::PrimaryId )
            {
                LOG_UNEXPECTED_READ;
                break;
            }

            assert ( primary->isServer() );
            LOG ( "Echoing 'PrimaryId' to '%s'", address.c_str() );
            secondary->send ( *msg, address );
            break;

        case State::Connecting:
            if ( msg->type() != MsgType::PrimaryId )
            {
                LOG_UNEXPECTED_READ;
                break;
            }

            LOG ( "Got 'PrimaryId' %08x", static_cast<PrimaryId *> ( msg.get() )->id );

            if ( socket == primary.get() )
            {
                static_cast<PrimaryId *> ( msg.get() )->sequence = 0;
                addMsgToResend ( msg );
            }
            else if ( socket == secondary.get() )
            {
                removeMsgToResend ( 0 );
            }
            else
            {
                LOG ( "Unknown socket %08x", socket );
            }
            break;

        case State::Connected:
            owner.readEvent ( this, msg, address );
            break;
    }
}

void DoubleSocket::addMsgToResend ( const MsgPtr& msg )
{
    LOG ( "Added '%s' to resend", TO_C_STR ( msg->type() ) );
    assert ( msg->base() == BaseType::SerializableSequence );
    resendList.push_back ( msg );
    resendTimer.start ( RESEND_INTERVAL );
}

void DoubleSocket::removeMsgToResend ( uint32_t sequence )
{
    LOG ( "Removing sequence=%u from resend", sequence );
    resendToDel.insert ( sequence );
}

void DoubleSocket::timerExpired ( Timer *timer )
{
    assert ( timer == &resendTimer );

    do
    {
        if ( resendList.empty() )
            break;

        if ( resendIter == resendList.end() )
            resendIter = resendList.begin();

        if ( resendToDel.find ( static_cast<SerializableSequence&> ( **resendIter ).sequence ) != resendToDel.end() )
        {
            LOG ( "Removed '%s' from resend", TO_C_STR ( ( **resendIter ).type() ) );
            resendList.erase ( resendIter );
            continue;
        }

        switch ( ( **resendIter ).type() )
        {
            default:
                if ( primary.get() )
                    primary->send ( **resendIter );
                else
                    LOG ( "Primary socket not initialized!" );
                break;

            case MsgType::PrimaryId:
                if ( secondary.get() )
                    secondary->send ( **resendIter );
                else
                    LOG ( "Secondary socket not initialized!" );
                break;
        }

        ++resendIter;
        resendTimer.start ( RESEND_INTERVAL );
    }
    while ( 0 );

    resendToDel.clear();
}

DoubleSocket::DoubleSocket ( Owner& owner, unsigned port )
    : owner ( owner )
    , state ( State::Listening )
    , primary ( Socket::listen ( *this, port, Protocol::TCP ) )
    , secondary ( Socket::listen ( *this, port, Protocol::UDP ) )
    , resendTimer ( *this )
    , resendIter ( resendList.end() )
{
    LOG ( "primary=%08x; secondary=%08x", primary.get(), secondary.get() );
}

DoubleSocket::DoubleSocket ( Owner& owner, const string& address, unsigned port )
    : owner ( owner )
    , state ( State::Connecting )
    , primary ( Socket::connect ( *this, address, port, Protocol::TCP ) )
    , secondary ( Socket::connect ( *this, address, port, Protocol::UDP ) )
    , resendTimer ( *this )
    , resendIter ( resendList.end() )
{
    LOG ( "primary=%08x; secondary=%08x", primary.get(), secondary.get() );
}

DoubleSocket *DoubleSocket::listen ( Owner& owner, unsigned port )
{
    return new DoubleSocket ( owner, port );
}

DoubleSocket *DoubleSocket::connect ( Owner& owner, const string& address, unsigned port )
{
    return new DoubleSocket ( owner, address, port );
}

DoubleSocket *DoubleSocket::relay ( Owner& owner, const string& room, const string& server, unsigned port )
{
    return 0;
}

DoubleSocket::~DoubleSocket()
{
    disconnect();
}

void DoubleSocket::disconnect()
{
    state = State::Disconnected;
    primary.reset();
    secondary.reset();
}

DoubleSocket *DoubleSocket::accept ( Owner& owner )
{
    // if ( !primary.get() )
    // return 0;

    // if ( primary->getProtocol() == Protocol::TCP )
    // return primary->accept ( owner );

    return 0;
}

void DoubleSocket::sendPrimary ( const Serializable& msg, const IpAddrPort& address )
{
    primary->send ( msg, address );
}

void DoubleSocket::sendSecondary ( const Serializable& msg, const IpAddrPort& address )
{
    secondary->send ( msg, address );
}

ostream& operator<< ( ostream& os, DoubleSocket::State state )
{
    switch ( state )
    {
        case DoubleSocket::State::Listening:
            return ( os << "Listening" );

        case DoubleSocket::State::Connecting:
            return ( os << "Connecting" );

        case DoubleSocket::State::Connected:
            return ( os << "Connected" );

        case DoubleSocket::State::Disconnected:
            return ( os << "Disconnected" );
    }

    return ( os << "Unknown state!" );
}
