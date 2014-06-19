#include "Event.h"
#include "Socket.h"
#include "Log.h"
#include "Util.h"

using namespace std;

Socket::Socket ( NL::Socket *socket )
    : owner ( 0 ), socket ( socket ), address ( socket )
    , protocol ( socket->protocol() == NL::TCP ? Protocol::TCP : Protocol::UDP )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner *owner, unsigned port, Protocol protocol )
    : owner ( owner ), address ( "", port ), protocol ( protocol )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner *owner, const string& address, unsigned port, Protocol protocol )
    : owner ( owner ), address ( address, port ), protocol ( protocol )
{
    EventManager::get().addSocket ( this );
}

Socket::~Socket()
{
    EventManager::get().removeSocket ( this );
}

void Socket::disconnect()
{
    EventManager::get().removeSocket ( this );
}

shared_ptr<Socket> Socket::listen ( Owner *owner, unsigned port, Protocol protocol )
{
    return shared_ptr<Socket> ( new Socket ( owner, port, protocol ) );
}

shared_ptr<Socket> Socket::connect ( Owner *owner, const string& address, unsigned port, Protocol protocol )
{
    return shared_ptr<Socket> ( new Socket ( owner, address, port, protocol ) );
}

shared_ptr<Socket> Socket::accept ( Owner *owner )
{
    if ( !acceptedSocket.get() )
        return shared_ptr<Socket>();

    acceptedSocket->owner = owner;
    return acceptedSocket;
}

void Socket::send ( const Serializable& msg, const IpAddrPort& address )
{
    string bytes = Serializable::encode ( msg );
    LOG ( "Encoded '%s' to [ %u bytes ]", TO_C_STR ( msg.type() ), bytes.size() );

    string base64;
    for ( char c : bytes )
        base64 += " " + toString ( "%02x", ( unsigned char ) c );
    LOG ( "Base64 :%s", base64.c_str() );

    send ( &bytes[0], bytes.size(), address );
}

void Socket::send ( char *bytes, size_t len, const IpAddrPort& address )
{
    if ( socket )
    {
        if ( address.empty() )
        {
            LOG ( "%s socket->send ( [ %u bytes ] ); address='%s'",
                  TO_C_STR ( socket->protocol() ), len, IpAddrPort ( socket ).c_str() );
            socket->send ( bytes, len );
        }
        else
        {
            LOG ( "%s socket->sendTo ( [ %u bytes ], '%s' )",
                  TO_C_STR ( socket->protocol() ), len, address.c_str() );
            socket->sendTo ( bytes, len, address.addr, address.port );
        }
    }
    else
    {
        LOG ( "Unconnected socket!" );
    }
}

ostream& operator<< ( ostream& os, const Protocol& protocol )
{
    switch ( protocol )
    {
        case Protocol::TCP:
            return ( os << "TCP" );

        case Protocol::UDP:
            return ( os << "UDP" );
    }

    return ( os << "Unknown protocol!" );
}

ostream& operator<< ( ostream& os, const NL::Protocol& protocol )
{
    switch ( protocol )
    {
        case NL::TCP:
            return ( os << "TCP" );

        case NL::UDP:
            return ( os << "UDP" );
    }

    return ( os << "Unknown protocol!" );
}
