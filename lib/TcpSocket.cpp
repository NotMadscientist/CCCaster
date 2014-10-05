#include "SocketManager.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>

using namespace std;


TcpSocket::TcpSocket ( Socket::Owner *owner, uint16_t port, bool isRaw )
    : Socket ( IpAddrPort ( "", port ), Protocol::TCP, isRaw )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw )
    : Socket ( address, Protocol::TCP, isRaw )
{
    this->owner = owner;
    this->state = State::Connecting;
    Socket::init();
    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address ) : Socket ( address, Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Connected;
    this->fd = fd;
    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const SocketShareData& data ) : Socket ( data.address, Protocol::TCP )
{
    ASSERT ( data.protocol == Protocol::TCP );

    this->owner = owner;
    this->state = data.state;
    this->readBuffer = data.readBuffer;
    this->readPos = data.readPos;

    ASSERT ( data.info->iSocketType == SOCK_STREAM );
    ASSERT ( data.info->iProtocol == IPPROTO_TCP );

    this->fd = WSASocket ( data.info->iAddressFamily, SOCK_STREAM, IPPROTO_TCP, data.info.get(), 0, 0 );

    if ( this->fd == INVALID_SOCKET )
    {
        WindowsException err = WSAGetLastError();
        LOG_SOCKET ( this, "%s; WSASocket failed", err );
        this->fd = 0;
        throw err;
    }

    SocketManager::get().add ( this );
}

TcpSocket::~TcpSocket()
{
    disconnect();
}

void TcpSocket::disconnect()
{
    SocketManager::get().remove ( this );
    Socket::disconnect();
}

SocketPtr TcpSocket::listen ( Socket::Owner *owner, uint16_t port, bool isRaw )
{
    return SocketPtr ( new TcpSocket ( owner, port, isRaw ) );
}

SocketPtr TcpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw )
{
    return SocketPtr ( new TcpSocket ( owner, address, isRaw ) );
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
        LOG_SOCKET ( this, "%s; accept failed", WindowsException ( WSAGetLastError() ) );
        return 0;
    }

    return SocketPtr ( new TcpSocket ( owner, newFd, IpAddrPort ( ( sockaddr * ) &sas ) ) );
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
    string buffer = ::Protocol::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() && buffer.size() <= 256 )
        LOG ( "Base64 : %s", toBase64 ( buffer ) );

    return Socket::send ( &buffer[0], buffer.size() );
}

SocketPtr TcpSocket::shared ( Socket::Owner *owner, const SocketShareData& data )
{
    if ( data.protocol != Protocol::TCP )
        return 0;

    return SocketPtr ( new TcpSocket ( owner, data ) );
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
