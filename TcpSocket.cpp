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

shared_ptr<Socket> TcpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return shared_ptr<Socket> ( new TcpSocket ( owner, port ) );
}

shared_ptr<Socket> TcpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return shared_ptr<Socket> ( new TcpSocket ( owner, address ) );
}

shared_ptr<Socket> TcpSocket::accept ( Socket::Owner *owner )
{
    if ( !isServer() )
        return 0;

    sockaddr_storage sa;
    int saLen = sizeof ( sa );

    int newFd = ::accept ( fd, ( struct sockaddr * ) &sa, &saLen );

    if ( newFd == INVALID_SOCKET )
    {
        LOG_SOCKET ( ( "accept failed: " + getLastWinSockError() ).c_str(), this );
        return 0;
    }

    if ( sa.ss_family != AF_INET )
    {
        closesocket ( newFd );
        LOG_SOCKET ( "Only accepting IPv4 from", this );
        return 0;
    }

    // TODO IPv6
    char newAddr[INET6_ADDRSTRLEN];
    inet_ntop ( sa.ss_family, & ( ( ( struct sockaddr_in * ) &sa )->sin_addr ), newAddr, sizeof ( newAddr ) );

    uint16_t newPort = ntohs ( ( ( struct sockaddr_in * ) &sa )->sin_port );

    return shared_ptr<Socket> ( new TcpSocket ( owner, newFd, IpAddrPort ( newAddr, newPort ) ) );
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

    LOG ( "Encoded '%s' to [ %u bytes ]", TO_C_STR ( msg ), buffer.size() );
    LOG ( "Base64 : %s", toBase64 ( buffer ).c_str() );

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
