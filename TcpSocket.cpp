#include "Event.h"
#include "TcpSocket.h"
#include "Log.h"
#include "Util.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>

using namespace std;

TcpSocket::TcpSocket ( Socket::Owner *owner, uint16_t port ) : Socket ( IpAddrPort ( "", port ), Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    EventManager::get().addSocket ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const IpAddrPort& address ) : Socket ( address, Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Connecting;
    Socket::init();
    EventManager::get().addSocket ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address ) : Socket ( address, Protocol::TCP )
{
    this->owner = owner;
    this->fd = fd;
    this->state = State::Connected;
    EventManager::get().addSocket ( this );
}

TcpSocket::~TcpSocket()
{
    disconnect();
}

void TcpSocket::disconnect()
{
    EventManager::get().removeSocket ( this );
    Socket::disconnect();
}

SocketPtr TcpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return SocketPtr ( new TcpSocket ( owner, port ) );
}

SocketPtr TcpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return SocketPtr ( new TcpSocket ( owner, address ) );
}

SocketPtr TcpSocket::accept ( Socket::Owner *owner )
{
    if ( !isServer() )
        return 0;

    sockaddr_storage sas;
    int saLen = sizeof ( sas );

    int newFd = ::accept ( fd, ( sockaddr * ) &sas, &saLen );

    if ( newFd == INVALID_SOCKET )
    {
        LOG_SOCKET ( this, "accept failed: %s", WindowsError ( WSAGetLastError() ) );
        return 0;
    }

    return SocketPtr ( new TcpSocket ( owner, newFd, ( sockaddr * ) &sas ) );
}

bool TcpSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ) );
}

bool TcpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ) );
}

bool TcpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    string buffer = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() )
        LOG ( "Base64 : %s", toBase64 ( buffer ) );

    return Socket::send ( &buffer[0], buffer.size() );
}

void TcpSocket::acceptEvent()
{
    if ( owner )
        owner->acceptEvent ( this );
    else
        accept ( 0 ).reset();
}

void TcpSocket::connectEvent()
{
    state = State::Connected;
    if ( owner )
        owner->connectEvent ( this );
}

void TcpSocket::disconnectEvent()
{
    Socket::Owner *owner = this->owner;
    disconnect();
    if ( owner )
        owner->disconnectEvent ( this );
}

void TcpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->readEvent ( this, msg, address );
}
