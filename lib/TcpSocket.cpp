#include "SocketManager.hpp"
#include "TcpSocket.hpp"
#include "Protocol.hpp"
#include "Exceptions.hpp"
#include "ErrorStrings.hpp"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>

using namespace std;


TcpSocket::TcpSocket ( Socket::Owner *owner, uint16_t port, bool isRaw )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::TCP, isRaw )
{
    _state = State::Listening;

    Socket::init();
    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw, uint64_t connectTimeout )
    : Socket ( owner, address, Protocol::TCP, isRaw )
{
    _state = State::Connecting;

    Socket::init();
    SocketManager::get().add ( this );

    _connectTimeout = connectTimeout;

    _connectTimer.reset ( new Timer ( this ) );
    _connectTimer->start ( connectTimeout );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address, bool isRaw )
    : Socket ( owner, address, Protocol::TCP, isRaw )
{
    _state = State::Connected;
    _fd = fd;

    SocketManager::get().add ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( owner, data.address, Protocol::TCP, data.isRaw )
{
    ASSERT ( data.protocol == Protocol::TCP );

    this->owner = owner;

    _connectTimeout = data.connectTimeout;
    _state = data.state;
    _readBuffer = data.readBuffer;
    _readPos = data.readPos;

    ASSERT ( data.info->iSocketType == SOCK_STREAM );
    ASSERT ( data.info->iProtocol == IPPROTO_TCP );

    _fd = WSASocket ( data.info->iAddressFamily, SOCK_STREAM, IPPROTO_TCP, data.info.get(), 0, 0 );

    if ( _fd == INVALID_SOCKET )
    {
        LOG_SOCKET ( this, "WSASocket failed" );
        _fd = 0;
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

    _connectTimer.reset();
}

SocketPtr TcpSocket::listen ( Socket::Owner *owner, uint16_t port, bool isRaw )
{
    return SocketPtr ( new TcpSocket ( owner, port, isRaw ) );
}

SocketPtr TcpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw, uint64_t connectTimeout )
{
    return SocketPtr ( new TcpSocket ( owner, address, isRaw, connectTimeout ) );
}

SocketPtr TcpSocket::accept ( Socket::Owner *owner )
{
    if ( ! isServer() )
        return 0;

    sockaddr_storage sas;
    int saLen = sizeof ( sas );

    const int newFd = ::accept ( _fd, ( sockaddr * ) &sas, &saLen );

    if ( newFd == INVALID_SOCKET )
    {
        const int error = WSAGetLastError();
        LOG_SOCKET ( this, "[%d] %s; accept failed", error, WinException::getAsString ( error ) );
        return 0;
    }

    return SocketPtr ( new TcpSocket ( owner, newFd, IpAddrPort ( ( sockaddr * ) &sas ), _isRaw ) );
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
    const string buffer = ::Protocol::encode ( msg );

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

void TcpSocket::socketAccepted()
{
    if ( owner )
        owner->socketAccepted ( this );
    else
        accept ( 0 ).reset();
}

void TcpSocket::socketConnected()
{
    _state = State::Connected;

    if ( owner )
        owner->socketConnected ( this );
}

void TcpSocket::socketDisconnected()
{
    Socket::Owner *const owner = this->owner;

    disconnect();

    if ( owner )
        owner->socketDisconnected ( this );
}

void TcpSocket::socketRead ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->socketRead ( this, msg, address );
}

void TcpSocket::timerExpired ( Timer *timer )
{
    ASSERT ( timer == _connectTimer.get() );

    _connectTimer.reset();

    if ( isConnected() )
        return;

    socketDisconnected();
}
