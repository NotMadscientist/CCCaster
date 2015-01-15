#include "SocketManager.h"
#include "TcpSocket.h"
#include "Protocol.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>

using namespace std;


TcpSocket::TcpSocket ( Socket::Owner *owner, uint16_t port, bool isRaw )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::TCP, isRaw )
{
    this->state = State::Listening;

    Socket::init();
    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw )
    : Socket ( owner, address, Protocol::TCP, isRaw )
{
    this->state = State::Connecting;

    Socket::init();
    SocketManager::get().add ( this );

    connectTimer.reset ( new Timer ( this ) );
    connectTimer->start ( connectTimeout );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address )
    : Socket ( owner, address, Protocol::TCP )
{
    this->state = State::Connected;
    this->fd = fd;

    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( owner, data.address, Protocol::TCP, data.isRaw )
{
    ASSERT ( data.protocol == Protocol::TCP );

    this->owner = owner;

    this->connectTimeout = data.connectTimeout;
    this->state = data.state;
    this->readBuffer = data.readBuffer;
    this->readPos = data.readPos;

    ASSERT ( data.info->iSocketType == SOCK_STREAM );
    ASSERT ( data.info->iProtocol == IPPROTO_TCP );

    this->fd = WSASocket ( data.info->iAddressFamily, SOCK_STREAM, IPPROTO_TCP, data.info.get(), 0, 0 );

    if ( this->fd == INVALID_SOCKET )
    {
        LOG_SOCKET ( this, "WSASocket failed" );
        this->fd = 0;
        THROW_WIN_EXCEPTION ( WSAGetLastError(), "WSASocket failed", ERROR_NETWORK_GENERIC );
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

    connectTimer.reset();
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
        int error = WSAGetLastError();
        LOG_SOCKET ( this, "[%d] %s; accept failed", error, WinException::getAsString ( error ) );
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
        LOG ( "Hex: %s", formatAsHex ( buffer ) );

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
    Socket::Owner *const owner = this->owner;

    disconnect();

    if ( owner )
        owner->disconnectEvent ( this );
}

void TcpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->readEvent ( this, msg, address );
}

void TcpSocket::timerExpired ( Timer *timer )
{
    ASSERT ( timer == connectTimer.get() );

    connectTimer.reset();

    if ( isConnected() )
        return;

    disconnectEvent();
}
