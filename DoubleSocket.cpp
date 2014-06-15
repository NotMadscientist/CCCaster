#include "DoubleSocket.h"
#include "Log.h"

#include <cassert>

#define RESEND_INTERVAL     100

using namespace std;

void DoubleSocket::acceptEvent ( Socket *serverSocket )
{
    assert ( serverSocket == primary.get() );

    // TODO proper accept
    Socket *socket = serverSocket->accept ( *this );

    assert ( socket );

    socket->send ( PrimaryId ( 0, ( uint32_t ) socket ) );
}

void DoubleSocket::connectEvent ( Socket *socket )
{
    assert ( socket == primary.get() );

    resendTimer.start ( RESEND_INTERVAL );
}

void DoubleSocket::disconnectEvent ( Socket *socket )
{
    // assert ( socket == primary.get() );
}

void DoubleSocket::readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
{
    MsgPtr msg = Serializable::decode ( bytes, len );

    if ( !msg.get() )
    {
        LOG ( "Failed to decode [ %u bytes ]", len );
    }
    else
    {
        switch ( msg->type() )
        {
            case MsgType::PrimaryId:
            {
                LOG ( "PrimaryId %08x", static_cast<PrimaryId *> ( msg.get() )->id );

                if ( socket == primary.get() )
                {
                    resendList.push_back ( msg );
                }
                else if ( primary->isServer() )
                {
                    secondary->send ( *msg, address );
                }
                else
                {
                    // TODO delete specific message
                    resendList.clear();
                }

                break;
            }

            default:
                owner.readEvent ( this, msg, address );
                break;
        }
    }
}

void DoubleSocket::timerExpired ( Timer *timer )
{
    assert ( timer == &resendTimer );

    // TODO sequence number and socket selection
    for ( const MsgPtr& msg : resendList )
        secondary->send ( *msg );

    if ( !resendList.empty() )
        resendTimer.start ( RESEND_INTERVAL );
}

DoubleSocket::DoubleSocket ( Owner& owner, unsigned port )
    : owner ( owner )
    , primary ( Socket::listen ( *this, port, Protocol::TCP ) )
    , secondary ( Socket::listen ( *this, port, Protocol::UDP ) )
    , resendTimer ( *this )
{
}

DoubleSocket::DoubleSocket ( Owner& owner, const string& address, unsigned port )
    : owner ( owner )
    , primary ( Socket::connect ( *this, address, port, Protocol::TCP ) )
    , secondary ( Socket::connect ( *this, address, port, Protocol::UDP ) )
    , resendTimer ( *this )
{
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
    primaryMap.clear();
    secondaryMap.clear();
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
