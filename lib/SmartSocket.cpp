#include "SmartSocket.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Logger.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

using namespace std;

// TODO add more state to this log macro
#define LOG_SMART_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, FORMAT, ## __VA_ARGS__)

#define CONNECT_TIMEOUT ( 2000 )

#define SEND_INTERVAL ( 50 )

const IpAddrPort vpsAddress = "23.95.23.238:3939";


SmartSocket::SmartSocket ( Socket::Owner *owner, Socket::Protocol protocol, uint16_t port )
    : Socket ( IpAddrPort ( "", port ), Protocol::Smart, false )
{
    vpsSocket = TcpSocket::connect ( this, vpsAddress );

    if ( protocol == Protocol::TCP )
        backingSocket = TcpSocket::listen ( this, port );
    else
        backingSocket = UdpSocket::listen ( this, port );
}

SmartSocket::SmartSocket ( Socket::Owner *owner, Socket::Protocol protocol, const IpAddrPort& address )
    : Socket ( address, Protocol::Smart, false )
{
    if ( protocol == Protocol::TCP )
        backingSocket = TcpSocket::connect ( this, address );
    else
        backingSocket = UdpSocket::connect ( this, address );

    connectTimer.reset ( new Timer ( this ) );
    connectTimer->start ( CONNECT_TIMEOUT );
}

SmartSocket::~SmartSocket()
{
    disconnect();
}

void SmartSocket::disconnect()
{
    Socket::disconnect();

    backingSocket.reset();
    vpsSocket.reset();
    tunSocket.reset();
    connectTimer.reset();
    sendTimer.reset();
}

void SmartSocket::acceptEvent ( Socket *serverSocket )
{
}

void SmartSocket::connectEvent ( Socket *socket )
{
}

void SmartSocket::disconnectEvent ( Socket *socket )
{
}

void SmartSocket::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
}

void SmartSocket::readEvent ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address )
{
    ASSERT ( socket == tunSocket.get() );
}

void SmartSocket::timerExpired ( Timer *timer )
{
    if ( timer == connectTimer.get() )
    {
        if ( backingSocket )
        {
            LOG_SMART_SOCKET ( this, "Switching to UDP tunnel" );

            backingSocket.reset();

            // vpsSocket = TcpSocket::connect ( this, vpsAddress ); // Need this?
            tunSocket = UdpSocket::bind ( this, vpsAddress, true ); // Raw socket

            connectTimer->start ( CONNECT_TIMEOUT );

            sendTimer.reset ( new Timer ( this ) );
            sendTimer->start ( SEND_INTERVAL );
        }
        else
        {
            ASSERT ( backingSocket.get() == 0 );
            ASSERT ( tunSocket.get() != 0 );

            LOG_SMART_SOCKET ( this, "disconnectEvent" );

            // Disconnect if we failed to connect via UDP tunnel
            Socket::Owner *owner = this->owner;

            disconnect();

            if ( owner )
                owner->disconnectEvent ( this );
        }
    }
    else
    {
        ASSERT ( timer == sendTimer.get() );
        ASSERT ( !address.addr.empty() );

        // TODO move this into the class def
        string buffer = address.str();
        buffer.resize ( buffer.size() + 1 );
        buffer[buffer.size() - 1] = '\0';

        tunSocket->send ( &buffer[0], buffer.size() );
    }
}

SocketPtr SmartSocket::listen ( Socket::Owner *owner, Socket::Protocol protocol, uint16_t port )
{
    return SocketPtr ( new SmartSocket ( owner, protocol, port ) );
}

SocketPtr SmartSocket::connect ( Socket::Owner *owner, Socket::Protocol protocol, const IpAddrPort& address )
{
    string addr = getAddrFromSockAddr ( address.getAddrInfo()->ai_addr ); // Resolve IP address first
    return SocketPtr ( new SmartSocket ( owner, protocol, { addr, address.port } ) );
}

SocketPtr SmartSocket::accept ( Socket::Owner *owner )
{
    return 0;
}

bool SmartSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    return false;
}

bool SmartSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return false;
}

bool SmartSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    return false;
}
