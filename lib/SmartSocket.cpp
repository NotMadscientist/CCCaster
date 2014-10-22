#include "SmartSocket.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Logger.h"

using namespace std;

#define CONNECT_TIMEOUT ( 5000 )


SmartSocket::SmartSocket ( Socket::Owner *owner, uint16_t port )
    : Socket ( IpAddrPort ( "", port ), Protocol::Smart, false )
{
}

SmartSocket::SmartSocket ( Socket::Owner *owner, const IpAddrPort& address )
    : Socket ( address, Protocol::Smart, false )
{
}

SmartSocket::~SmartSocket()
{
    disconnect();
}

void SmartSocket::disconnect()
{
    Socket::disconnect();
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
}

void SmartSocket::timerExpired ( Timer *timer )
{
    ASSERT ( timer == connectTimer.get() );
}

SocketPtr SmartSocket::listen ( Socket::Owner *owner, Socket::Protocol protocol, uint16_t port )
{
    return 0;
}

SocketPtr SmartSocket::connect ( Socket::Owner *owner, Socket::Protocol protocol, const IpAddrPort& address )
{
    return 0;
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
