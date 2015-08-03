#include "SocketManager.hpp"
#include "UdpSocket.hpp"
#include "Protocol.hpp"
#include "Exceptions.hpp"
#include "ErrorStrings.hpp"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <typeinfo>
#include <algorithm>

using namespace std;


#define LOG_UDP_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, "type=%s; " FORMAT, _type, ## __VA_ARGS__)


UdpSocket::UdpSocket ( Socket::Owner *owner, uint16_t port, const Type& type, bool isRaw )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::UDP, isRaw )
    , _type ( type )
    , _gbn ( this, DEFAULT_SEND_INTERVAL, isConnectionLess() ? 0 : DEFAULT_KEEP_ALIVE_TIMEOUT )
{
    _state = State::Listening;

    Socket::init();
    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, const Type& type,
                       bool isRaw, uint64_t connectTimeout )
    : Socket ( owner, address, Protocol::UDP, isRaw )
    , _type ( type )
    , _gbn ( this, DEFAULT_SEND_INTERVAL, isConnectionLess() ? 0 : connectTimeout )
{
    _state = ( isConnectionLess() ? State::Connected : State::Connecting );

    Socket::init();
    SocketManager::get().add ( this );

    _connectTimeout = connectTimeout;

    if ( isConnecting() )
        send ( new UdpControl ( UdpControl::ConnectRequest ) );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( owner, data.address, Protocol::UDP, data.isRaw )
    , _type ( ( Type::Enum ) data.udpType )
    , _gbn ( this )
{
    ASSERT ( data.protocol == Protocol::UDP );

    _connectTimeout = data.connectTimeout;
    _state = data.state;
    _readBuffer = data.readBuffer;
    _readPos = data.readPos;

    ASSERT ( data.info->iSocketType == SOCK_DGRAM );
    ASSERT ( data.info->iProtocol == IPPROTO_UDP );

    _fd = WSASocket ( data.info->iAddressFamily, SOCK_DGRAM, IPPROTO_UDP, data.info.get(), 0, 0 );

    if ( _fd == INVALID_SOCKET )
    {
        LOG_UDP_SOCKET ( this, "WSASocket failed" );
        _fd = 0;
        THROW_WIN_EXCEPTION ( WSAGetLastError(), "WSASocket failed", ERROR_NETWORK_GENERIC );
    }

    LOG ( "Shared:" );

    switch ( _type.value )
    {
        case Type::ConnectionLess:
            LOG ( "connection-less: address='%s'", address );
            break;

        case Type::Client:
            _gbn = data.gbnState->getAs<GoBackN>();

            LOG ( "client: address='%s'; keepAlive=%d", address, _keepAlive );
            _gbn.logSendList();
            break;

        case Type::Server:
            _gbn = data.gbnState->getAs<GoBackN>();

            LOG ( "server: address='%s'; keepAlive=%d", address, _keepAlive );
            _gbn.logSendList();

            for ( const auto& kv : data.childSockets )
            {
                UdpSocket *socket = new UdpSocket ( ChildSocket, this, kv.first, kv.second );
                _childSockets.insert ( make_pair ( socket->address, SocketPtr ( socket ) ) );

                LOG ( "child: address='%s'; keepAlive=%d", socket->address, socket->_keepAlive );
                socket->_gbn.logSendList();
            }
            break;

        default:
            THROW_EXCEPTION ( "Invalid UDP socket type!", ERROR_INTERNAL );
    }

    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address )
    : Socket ( 0, address, Protocol::UDP, parentSocket->_isRaw )
    , _type ( Type::Child )
    , _gbn ( this, parentSocket->getSendInterval(), parentSocket->_connectTimeout )
    , _parentSocket ( parentSocket )
{
    _state = State::Connecting;
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address, const GoBackN& state )
    : Socket ( 0, address, Protocol::UDP, parentSocket->_isRaw )
    , _type ( Type::Child )
    , _gbn ( this, state )
    , _parentSocket ( parentSocket )
{
    _state = State::Connected;
}

UdpSocket::~UdpSocket()
{
    disconnect();
}

void UdpSocket::disconnect()
{
    // Send 3 UdpControl::Disconnect messages if not connection-less
    if ( !isConnectionLess() && ( isConnected() || isServer() ) )
    {
        MsgPtr msg ( new UdpControl ( UdpControl::Disconnect ) );

        if ( isClient() )
        {
            for ( int i = 0; i < 3; ++i )
                send ( msg );
        }
        else if ( isServer() )
        {
            for ( int i = 0; i < 3; ++i )
            {
                for ( auto& kv : _childSockets )
                    kv.second->send ( msg );
            }
        }
    }

    // Real UDP sockets need to be removed on disconnect
    if ( isReal() )
        SocketManager::get().remove ( this );

    Socket::disconnect();

    _gbn.reset();
    _gbn.setKeepAlive ( 0 );

    // Detach child sockets first
    for ( auto& kv : _childSockets )
        kv.second->getAsUDP()._parentSocket = 0;

    // Check and remove child from parent
    if ( _parentSocket != 0 )
    {
        _parentSocket->_childSockets.erase ( getRemoteAddress() );
        _parentSocket = 0;
    }
}

SocketPtr UdpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return SocketPtr ( new UdpSocket ( owner, port, Type::Server, false ) );
}

SocketPtr UdpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address, uint64_t connectTimeout )
{
    return SocketPtr ( new UdpSocket ( owner, address, Type::Client, false, connectTimeout ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, uint16_t port, bool isRaw )
{
    return SocketPtr ( new UdpSocket ( owner, port, Type::ConnectionLess, isRaw ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw )
{
    return SocketPtr ( new UdpSocket ( owner, address, Type::ConnectionLess, isRaw, DEFAULT_CONNECT_TIMEOUT ) );
}

SocketPtr UdpSocket::shared ( Socket::Owner *owner, const SocketShareData& data )
{
    if ( data.protocol != Protocol::UDP )
        return 0;

    return SocketPtr ( new UdpSocket ( owner, data ) );
}

SocketPtr UdpSocket::accept ( Socket::Owner *owner )
{
    if ( ! _acceptedSocket.get() )
        return 0;

    _acceptedSocket->owner = owner;

    SocketPtr ret;
    _acceptedSocket.swap ( ret );
    return ret;
}

bool UdpSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    _gbn.delayKeepAliveOnce();
    return sendRaw ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( isConnectionLess() || msg.get() == 0
            // Send the UdpControl::Disconnect message immediately instead of using GoBackN
            || ( msg->getMsgType() == MsgType::UdpControl
                 && msg->getAs<UdpControl>().value == UdpControl::Disconnect ) )
    {
        _gbn.delayKeepAliveOnce();
        return sendRaw ( msg, address );
    }

    switch ( msg->getBaseType().value )
    {
        case BaseType::SerializableMessage:
            _gbn.delayKeepAliveOnce();
            return sendRaw ( msg, address );

        case BaseType::SerializableSequence:
            _gbn.sendViaGoBackN ( msg );
            return isConnected();

        default:
            LOG ( "Unhandled BaseType '%s'!", msg->getBaseType() );
            return false;
    }
}

bool UdpSocket::sendRaw ( const MsgPtr& msg, const IpAddrPort& address )
{
#ifndef RELEASE
    // Simulate hash fail
    if ( _hashFailRate && msg )
    {
        if ( rand() % 100 < _hashFailRate )
        {
            LOG ( "Munging hash for '%s'", msg );
            for ( char& byte : msg->_hash )
                byte = ( rand() % 0x100 );
            msg->_hashValid = false;
        }
        else
        {
            msg->_hashValid = true;
        }
    }
#endif // NOT RELEASE

    const string buffer = ::Protocol::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() && buffer.size() <= 256 )
        LOG ( "Hex: %s", formatAsHex ( buffer ) );

    // Real UDP sockets send directly
    if ( isReal()  )
        return Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    // Child UDP sockets send via parent if not disconnected
    if ( isChild() && _parentSocket )
        return _parentSocket->Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    LOG_UDP_SOCKET ( this, "Cannot send over disconnected socket" );
    return false;
}

void UdpSocket::goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &_gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    sendRaw ( msg, getRemoteAddress() );
}

void UdpSocket::goBackNRecvRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &_gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    if ( owner )
        owner->socketRead ( this, msg, getRemoteAddress() );
}

void UdpSocket::goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &_gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    // Child sockets handle GoBackN messages as a proxy of the parent socket,
    // this is so the GoBackN state resides in the child socket.
    if ( isChild() )
    {
        ASSERT ( _parentSocket->_childSockets.find ( getRemoteAddress() ) != _parentSocket->_childSockets.end() );
        ASSERT ( _parentSocket->_childSockets[getRemoteAddress()].get() == this );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpControl:
                if ( isConnecting() )
                {
                    switch ( msg->getAs<UdpControl>().value )
                    {
                        case UdpControl::ConnectRequest:
                            // UdpControl::ConnectRequest should be responded to with a Reply
                            send ( new UdpControl ( UdpControl::ConnectReply ) );
                            return;

                        case UdpControl::ConnectFinal:
                            // UdpControl::ConnectFinal means the client connected properly and is now accepted
                            _state = State::Connected;

                            LOG_UDP_SOCKET ( this, "socketAccepted" );

                            _parentSocket->_acceptedSocket = _parentSocket->_childSockets[getRemoteAddress()];

                            _gbn.setKeepAlive ( _keepAlive );

                            if ( _parentSocket->owner )
                                _parentSocket->owner->socketAccepted ( _parentSocket );
                            return;

                        default:
                            return;
                    }
                    return;
                }

                if ( msg->getAs<UdpControl>().value == UdpControl::Disconnect )
                {
                    LOG_UDP_SOCKET ( this, "socketDisconnected" );

                    Socket::Owner *const owner = this->owner;

                    disconnect();

                    if ( owner )
                        owner->socketDisconnected ( this );
                    return;
                }

                LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
                return;

            default:
                if ( owner )
                    owner->socketRead ( this, msg, getRemoteAddress() );
                return;
        }
    }
    else
    {
        ASSERT ( isReal() == true );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpControl:
                // UdpControl::ConnectReply while connecting indicates this socket is now connected
                if ( isConnecting() && msg->getAs<UdpControl>().value == UdpControl::ConnectReply )
                {
                    _state = State::Connected;

                    LOG_UDP_SOCKET ( this, "socketConnected" );

                    send ( new UdpControl ( UdpControl::ConnectFinal ) );

                    _gbn.setKeepAlive ( _keepAlive );

                    if ( owner )
                        owner->socketConnected ( this );
                    return;
                }

                if ( msg->getAs<UdpControl>().value == UdpControl::Disconnect )
                {
                    LOG_UDP_SOCKET ( this, "socketDisconnected" );

                    Socket::Owner *const owner = this->owner;

                    disconnect();

                    if ( owner )
                        owner->socketDisconnected ( this );
                    return;
                }

                LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
                return;

            default:
                if ( owner )
                    owner->socketRead ( this, msg, getRemoteAddress() );
                return;
        }
    }
}

void UdpSocket::goBackNTimeout ( GoBackN *gbn )
{
    ASSERT ( gbn == &_gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    LOG_UDP_SOCKET ( this, "socketDisconnected" );

    Socket::Owner *const owner = this->owner;

    disconnect();

    if ( owner )
        owner->socketDisconnected ( this );
}

void UdpSocket::socketRead ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( isConnectionLess() )
    {
        // Recv directly if we're in connection-less mode
        if ( owner )
            owner->socketRead ( this, msg, address );
    }
    else if ( isClient() )
    {
        // Handle the UdpControl::Disconnect message immediately instead of using GoBackN
        if ( msg.get() != 0
                && msg->getMsgType() == MsgType::UdpControl
                && msg->getAs<UdpControl>().value == UdpControl::Disconnect )
        {
            goBackNRecvMsg ( &_gbn, msg );
            return;
        }

        // Client UDP sockets recv into the GoBackN instance
        _gbn.recvFromSocket ( msg );
    }
    else if ( isServer() )
    {
        // Server UDP sockets recv into the addressed child socket
        socketReadAddressed ( msg, address );
    }
    else
    {
        LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
    }
}

void UdpSocket::socketReadAddressed ( const MsgPtr& msg, const IpAddrPort& address )
{
    UdpSocket *socket;

    const auto it = _childSockets.find ( address );
    if ( it != _childSockets.end() )
    {
        // Get the existing child socket
        socket = & ( it->second->getAsUDP() );
    }
    else if ( msg.get()
              && msg->getMsgType() == MsgType::UdpControl
              && msg->getAs<UdpControl>().value == UdpControl::ConnectRequest )
    {
        // Only a connect request is allowed to open a new child socket
        socket = new UdpSocket ( ChildSocket, this, address );
        _childSockets.insert ( make_pair ( address, SocketPtr ( socket ) ) );
    }
    else
    {
        LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
        LOG_UDP_SOCKET ( this, "Ignoring because no child socket for '%s'", address );
        return;
    }

    ASSERT ( socket != 0 );

    socket->socketRead ( msg, address );
}

MsgPtr UdpSocket::share ( int processId )
{
    if ( isChild() )
        return NullMsg;

    MsgPtr data = Socket::share ( processId );

    ASSERT ( typeid ( *data ) == typeid ( SocketShareData ) );
    ASSERT ( isReal() == true );

    LOG ( "Sharing UDP:" );

    switch ( _type.value )
    {
        case Type::ConnectionLess:
            LOG ( "connection-less: address='%s'", address );
            break;

        case Type::Client:
            LOG ( "client: address='%s'; keepAlive=%d", address, _keepAlive );
            _gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( _gbn ) );
            break;

        case Type::Server:
            LOG ( "server: address='%s'; keepAlive=%d", address, _keepAlive );
            _gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( _gbn ) );

            for ( const auto& kv : _childSockets )
            {
                LOG ( "child: address='%s'; keepAlive=%d", kv.first, kv.second->getAsUDP()._keepAlive );
                kv.second->getAsUDP()._gbn.logSendList();

                data->getAs<SocketShareData>().childSockets[kv.first] = kv.second->getAsUDP()._gbn;
                kv.second->getAsUDP()._gbn.reset(); // Reset to stop the GoBackN timers from firing
            }
            break;

        default:
            return NullMsg;
    }

    data->getAs<SocketShareData>().udpType = _type.value;
    _gbn.reset(); // Reset to stop the GoBackN timers from firing

    return data;
}

void UdpSocket::listen()
{
    if ( _type == Type::Server )
        return;

    resetBuffer();

    ASSERT ( address.addr.empty() == true );
    ASSERT ( _parentSocket == 0 );
    ASSERT ( _childSockets.empty() == true );
    ASSERT ( _acceptedSocket.get() == 0 );

    _isRaw = false;
    _type = Type::Server;
    _gbn.setSendInterval ( DEFAULT_SEND_INTERVAL );
    _gbn.setKeepAlive ( DEFAULT_KEEP_ALIVE_TIMEOUT );
    _gbn.reset();
}

void UdpSocket::connect()
{
    if ( _type == Type::Client )
        return;

    resetBuffer();

    ASSERT ( address.addr.empty() == false );
    ASSERT ( _parentSocket == 0 );
    ASSERT ( _childSockets.empty() == true );
    ASSERT ( _acceptedSocket.get() == 0 );

    _isRaw = false;
    _type = Type::Client;
    _state = State::Connecting;
    _gbn.setSendInterval ( DEFAULT_SEND_INTERVAL );
    _gbn.setKeepAlive ( DEFAULT_KEEP_ALIVE_TIMEOUT );
    _gbn.reset();

    send ( new UdpControl ( UdpControl::ConnectRequest ) );
}

void UdpSocket::connect ( const IpAddrPort& address )
{
    this->address = address;
    connect();
}

void UdpSocket::setSendInterval ( uint64_t interval )
{
    if ( ! isConnectionLess() )
        _gbn.setSendInterval ( interval );
}

void UdpSocket::setKeepAlive ( uint64_t timeout )
{
    if ( ! isConnectionLess() )
        _gbn.setKeepAlive ( _keepAlive = timeout );
}

void UdpSocket::resetGbnState()
{
    _gbn.reset();
}
