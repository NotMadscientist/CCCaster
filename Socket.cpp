#include "Event.h"
#include "Socket.h"
#include "Log.h"
#include "Util.h"

#define READ_BUFFER_SIZE ( 1024 * 4096 )

using namespace std;

Socket::Socket ( NL::Socket *socket )
    : owner ( 0 ), socket ( socket ), protocol ( socket->protocol() == NL::TCP ? Protocol::TCP : Protocol::UDP )
    , readBuffer ( READ_BUFFER_SIZE, ( char ) 0 ), readPos ( 0 ), packetLoss ( 0 ), address ( socket )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner *owner, unsigned port, Protocol protocol )
    : owner ( owner ), protocol ( protocol ), readBuffer ( READ_BUFFER_SIZE, ( char ) 0 ), readPos ( 0 )
    , packetLoss ( 0 ), address ( "", port )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner *owner, const string& address, unsigned port, Protocol protocol )
    : owner ( owner ), protocol ( protocol ), readBuffer ( READ_BUFFER_SIZE, ( char ) 0 ), readPos ( 0 )
    , packetLoss ( 0 ), address ( address, port )
{
    EventManager::get().addSocket ( this );
}

Socket::~Socket()
{
    EventManager::get().removeSocket ( this );
}

void Socket::disconnect()
{
    readBuffer.clear();
    readPos = 0;
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
        return 0;

    acceptedSocket->owner = owner;
    return acceptedSocket;
}

void Socket::send ( Serializable *message, const IpAddrPort& address )
{
    MsgPtr msg ( message );
    send ( msg, address );
}

void Socket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    string bytes = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", TO_C_STR ( msg ), bytes.size() );
    LOG ( "Base64 : %s", toBase64 ( bytes ).c_str() );

    send ( &bytes[0], bytes.size(), address );
}

void Socket::send ( char *bytes, size_t len, const IpAddrPort& address )
{
    if ( socket.get() )
    {
        if ( address.empty() && isServer() )
        {
            LOG ( "Cannot send over server %s socket with no address!", TO_C_STR ( protocol ) );
        }
        else if ( address.empty() )
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

void Socket::setPacketLoss ( uint8_t percentage ) { packetLoss = percentage; }

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
