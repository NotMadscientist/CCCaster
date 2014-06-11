#include "Socket.h"
#include "EventManager.h"
#include "Log.h"

using namespace std;

#define UDP_BIND_ATTEMPTS   10

#define PORT_MIN            4096
#define PORT_MAX            65535

#define RANDOM_PORT         ( PORT_MIN + rand() % ( PORT_MAX - PORT_MIN ) )

Socket::Socket ( Owner& owner, NL::Socket *socket )
    : owner ( owner ), socket ( socket )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner& owner, const shared_ptr<NL::Socket>& socket )
    : owner ( owner ), socket ( socket )
{
    EventManager::get().addSocket ( this );
}

Socket::Socket ( Owner& owner, const string& address, unsigned port )
    : owner ( owner )
{
    EventManager::get().connectTcpSocket ( this, IpAddrPort ( address, port ) );
}

Socket::~Socket()
{
    disconnect();
}

shared_ptr<Socket> Socket::listen ( Owner& owner, unsigned port, Proto protocol )
{
    LOG ( "Binding socket to port %u", port );
    return shared_ptr<Socket> (
               new Socket ( owner, new NL::Socket ( port, protocol == TCP ? NL::TCP : NL::UDP, NL::IP4 ) ) );
}

shared_ptr<Socket> Socket::connect ( Owner& owner, const string& address, unsigned port, Proto protocol )
{
    if ( protocol == TCP )
    {
        LOG ( "Connecting TCP socket; address='%s:%u'", address.c_str(), port );
        return shared_ptr<Socket> ( new Socket ( owner, address, port ) );
    }
    else
    {
        shared_ptr<NL::Socket> socket;

        for ( int i = 0; i < UDP_BIND_ATTEMPTS; ++i )
        {
            unsigned port = RANDOM_PORT;
            LOG ( "Trying to bind local UDP port %u", port );

            try
            {
                socket.reset ( new NL::Socket ( address, port, RANDOM_PORT, NL::IP4 ) );
                break;
            }
            catch ( ... )
            {
                if ( i + 1 == UDP_BIND_ATTEMPTS )
                    throw;
                else
                    continue;
            }
        }

        LOG ( "New UDP socket; address='%s:%u'", address.c_str(), port );
        return shared_ptr<Socket> ( new Socket ( owner, socket ) );
    }
}

shared_ptr<Socket> Socket::accept ( Owner& owner )
{
    if ( socket->type() != NL::SERVER )
        return shared_ptr<Socket>();

    shared_ptr<NL::Socket> rawSocket ( socket->accept() );

    LOG ( "Accepted TCP socket; address='%s'", IpAddrPort ( rawSocket ).c_str() );
    return shared_ptr<Socket> ( new Socket ( owner, rawSocket ) );
}

void Socket::disconnect()
{
    // if ( !socket.get() )
    // return;

    // EventManager::get().removeSocket ( this );
    // socket.reset();
}

bool Socket::isConnected() const
{
    return socket.get();
}

IpAddrPort Socket::getRemoteAddress() const
{
    if ( !socket.get() )
        return IpAddrPort();

    return IpAddrPort ( socket );
}

void Socket::send ( const Serializable& msg, const IpAddrPort& address )
{
    string bytes = Serializable::encode ( msg );
    send ( &bytes[0], bytes.size(), address );
}

void Socket::send ( char *bytes, size_t len, const IpAddrPort& address )
{
    if ( socket.get() )
    {
        if ( address.empty() )
        {
            LOG ( "socket->send ( [ %u bytes ] ); address='%s'", len, IpAddrPort ( socket ).c_str() );
            socket->send ( bytes, len );
        }
        else
        {
            LOG ( "socket->sendTo ( [ %u bytes ], '%s' )", len, address.c_str() );
            socket->sendTo ( bytes, len, address.addr, address.port );
        }
    }
    else
    {
        LOG ( "Unconnected socket!" );
    }
}
