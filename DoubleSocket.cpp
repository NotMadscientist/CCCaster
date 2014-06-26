#include "DoubleSocket.h"
#include "Log.h"
#include "Util.h"

using namespace std;

#define LOG_UNEXPECTED_READ                                                                                     \
    LOG ( "Unexpected '%s' from %s socket %08x address '%s' while '%s'",                                        \
          TO_C_STR ( msg ), TO_C_STR ( socket->getProtocol() ), socket, address.c_str(), TO_C_STR ( state ) )

#define LOG_SOCKET(VERB, SOCKET)                                                                                \
    LOG ( "%s %s socket %08x { '%s' }",                                                                         \
          VERB, TO_C_STR ( SOCKET->getProtocol() ), SOCKET, SOCKET->getRemoteAddress().c_str() )

void DoubleSocket::acceptEvent ( Socket *serverSocket )
{
    assert ( serverSocket == primary.get() );

    shared_ptr<Socket> socket ( serverSocket->accept ( this ) );

    if ( state != State::Listening )
        return;

    assert ( socket.get() );

    LOG_SOCKET ( "Accepted", socket.get() );

    pendingAccepts[socket->getRemoteAddress()] = socket;
    socket->send ( new PrimaryId ( ( uint32_t ) socket.get() ) );
}

void DoubleSocket::connectEvent ( Socket *socket )
{
}

void DoubleSocket::disconnectEvent ( Socket *socket )
{
}

void DoubleSocket::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    if ( Log::isEnabled )
    {
        char buffer[1024];
        sprintf ( buffer, "Read '%s' from '%s';", TO_C_STR ( msg ), address.c_str() );
        LOG_SOCKET ( buffer, socket );
    }

    if ( socket == secondary.get() )
    {
        auto it = secondaryPorts.find ( address.port );

        if ( it == secondaryPorts.end() )
            LOG ( "Ignoring UDP message with unmapped port %u!", address.port );
        else
            it->second->owner->readEvent ( this, msg, false );

        return;
    }
}

void DoubleSocket::sendGoBackN ( const MsgPtr& msg )
{
    // While connecting, PrimaryId is sent over the secondary socket via Go-Back-N
    if ( state == State::Connecting )
    {
        // secondary->send ( msg );
        return;
    }

    // Otherwise this is a primary socket send
    primary->send ( msg );
}

void DoubleSocket::recvGoBackN ( const MsgPtr& msg )
{
    // While connecting, PrimaryId is received over the secondary socket via Go-Back-N
    if ( state == State::Connecting )
    {
        return;
    }

    // Otherwise this is a primary socket read
    owner->readEvent ( this, msg, true );
}

DoubleSocket::DoubleSocket ( Owner *owner, unsigned port )
    : owner ( owner )
    , state ( State::Listening )
    , primary ( Socket::listen ( this, port, Protocol::TCP ) )
    , secondary ( Socket::listen ( this, port, Protocol::UDP ) )
    , gbn ( this )
{
    LOG ( "this=%08x; primary=%08x; secondary=%08x", this, primary.get(), secondary.get() );
}

DoubleSocket::DoubleSocket ( Owner *owner, const string& address, unsigned port )
    : owner ( owner )
    , state ( State::Connecting )
    , primary ( Socket::connect ( this, address, port, Protocol::TCP ) )
    , secondary ( Socket::connect ( this, address, port, Protocol::UDP ) )
    , gbn ( this )
{
    LOG ( "this=%08x; primary=%08x; secondary=%08x", this, primary.get(), secondary.get() );
}

DoubleSocket::DoubleSocket ( const shared_ptr<Socket>& primary, const shared_ptr<Socket>& secondary )
    : owner ( 0 )
    , state ( State::Connecting )
    , primary ( primary )
    , secondary ( secondary )
    , gbn ( this )
{
    LOG ( "this=%08x; primary=%08x; secondary=%08x", this, primary.get(), secondary.get() );
}

shared_ptr<DoubleSocket> DoubleSocket::listen ( Owner *owner, unsigned port )
{
    return shared_ptr<DoubleSocket> ( new DoubleSocket ( owner, port ) );
}

shared_ptr<DoubleSocket> DoubleSocket::connect ( Owner *owner, const string& address, unsigned port )
{
    return shared_ptr<DoubleSocket> ( new DoubleSocket ( owner, address, port ) );
}

shared_ptr<DoubleSocket> DoubleSocket::relay ( Owner *owner, const string& room, const string& server, unsigned port )
{
    return shared_ptr<DoubleSocket>();
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
    gbn.reset();
}

shared_ptr<DoubleSocket> DoubleSocket::accept ( Owner *owner )
{
    if ( !acceptedSocket.get() )
        return shared_ptr<DoubleSocket>();

    acceptedSocket->owner = owner;
    return acceptedSocket;
}

void DoubleSocket::sendPrimary ( const MsgPtr& msg )
{
    assert ( primary.get() );
    assert ( msg->getBaseType() == BaseType::SerializableMessage );

    if ( primary->getProtocol() == Protocol::TCP )
        primary->send ( msg );
    else
        gbn.send ( msg );
}

void DoubleSocket::sendPrimary ( SerializableSequence *msg )
{
    sendPrimary ( MsgPtr ( msg ) );
}

void DoubleSocket::sendSecondary ( const MsgPtr& msg )
{
    assert ( secondary.get() );
    assert ( msg->getBaseType() == BaseType::SerializableMessage );
    secondary->send ( msg );
}

void DoubleSocket::sendSecondary ( SerializableMessage *msg )
{
    sendSecondary ( MsgPtr ( msg ) );
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
