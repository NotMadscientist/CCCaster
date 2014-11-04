#include "SocketManager.h"
#include "UdpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <typeinfo>
#include <algorithm>

using namespace std;


#define LOG_UDP_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, "type=%s; " FORMAT, type, ## __VA_ARGS__)


UdpSocket::UdpSocket ( Socket::Owner *owner, uint16_t port, const Type& type, bool isRaw )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::UDP, isRaw )
    , type ( type )
    , gbn ( this, DEFAULT_SEND_INTERVAL, isConnectionLess() ? 0 : DEFAULT_KEEP_ALIVE )
{
    this->state = State::Listening;

    Socket::init();
    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, const Type& type, bool isRaw )
    : Socket ( owner, address, Protocol::UDP, isRaw )
    , type ( type )
    , gbn ( this, DEFAULT_SEND_INTERVAL, isConnectionLess() ? 0 : connectTimeout )
{
    this->state = ( isConnectionLess() ? State::Connected : State::Connecting );

    Socket::init();
    SocketManager::get().add ( this );

    if ( isConnecting() )
        send ( new UdpConnect ( UdpConnect::Request ) );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( owner, data.address, Protocol::UDP, data.isRaw )
    , type ( ( Type::Enum ) data.udpType )
    , gbn ( this )
{
    ASSERT ( data.protocol == Protocol::UDP );

    this->connectTimeout = data.connectTimeout;
    this->state = data.state;
    this->readBuffer = data.readBuffer;
    this->readPos = data.readPos;

    ASSERT ( data.info->iSocketType == SOCK_DGRAM );
    ASSERT ( data.info->iProtocol == IPPROTO_UDP );

    this->fd = WSASocket ( data.info->iAddressFamily, SOCK_DGRAM, IPPROTO_UDP, data.info.get(), 0, 0 );

    if ( this->fd == INVALID_SOCKET )
    {
        WindowsException err = WSAGetLastError();
        LOG_UDP_SOCKET ( this, "%s; WSASocket failed", err );
        this->fd = 0;
        throw err;
    }

    LOG ( "Shared:" );

    switch ( type.value )
    {
        case Type::ConnectionLess:
            LOG ( "connection-less: address='%s'", address );
            break;

        case Type::Client:
            gbn = data.gbnState->getAs<GoBackN>();

            LOG ( "client: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();
            break;

        case Type::Server:
            gbn = data.gbnState->getAs<GoBackN>();

            LOG ( "server: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();

            for ( const auto& kv : data.childSockets )
            {
                UdpSocket *socket = new UdpSocket ( ChildSocket, this, kv.first, kv.second );
                childSockets.insert ( make_pair ( socket->address, SocketPtr ( socket ) ) );

                LOG ( "child: address='%s'; keepAlive=%d", socket->address, socket->getKeepAlive() );
                socket->gbn.logSendList();
            }
            break;

        default:
            LOG_AND_THROW_STRING ( "Invalid UDP socket type!" );
            break;
    }

    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address )
    : Socket ( 0, address, Protocol::UDP )
    , type ( Type::Child )
    , gbn ( this, parentSocket->getSendInterval(), parentSocket->connectTimeout )
    , parentSocket ( parentSocket )
{
    this->state = State::Connecting;
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address, const GoBackN& state )
    : Socket ( 0, address, Protocol::UDP )
    , type ( Type::Child )
    , gbn ( this, state )
    , parentSocket ( parentSocket )
{
    this->state = State::Connected;
}

UdpSocket::~UdpSocket()
{
    disconnect();
}

void UdpSocket::disconnect()
{
    // Real UDP sockets need to be removed on disconnect
    if ( isReal() )
        SocketManager::get().remove ( this );

    Socket::disconnect();

    gbn.reset();
    gbn.setKeepAlive ( 0 );

    // Detach child sockets first
    for ( auto& kv : childSockets )
        kv.second->getAsUDP().parentSocket = 0;

    // Check and remove child from parent
    if ( parentSocket != 0 )
    {
        parentSocket->childSockets.erase ( getRemoteAddress() );
        parentSocket = 0;
    }
}

SocketPtr UdpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return SocketPtr ( new UdpSocket ( owner, port, Type::Server ) );
}

SocketPtr UdpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return SocketPtr ( new UdpSocket ( owner, address, Type::Client ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, uint16_t port, bool isRaw )
{
    return SocketPtr ( new UdpSocket ( owner, port, Type::ConnectionLess, isRaw ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw )
{
    return SocketPtr ( new UdpSocket ( owner, address, Type::ConnectionLess, isRaw ) );
}

SocketPtr UdpSocket::shared ( Socket::Owner *owner, const SocketShareData& data )
{
    if ( data.protocol != Protocol::UDP )
        return 0;

    return SocketPtr ( new UdpSocket ( owner, data ) );
}

SocketPtr UdpSocket::accept ( Socket::Owner *owner )
{
    if ( !acceptedSocket.get() )
        return 0;

    acceptedSocket->owner = owner;

    SocketPtr ret;
    acceptedSocket.swap ( ret );
    return ret;
}

bool UdpSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    gbn.delayKeepAliveOnce();
    return sendRaw ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( isConnectionLess() || !msg.get() )
    {
        gbn.delayKeepAliveOnce();
        return sendRaw ( msg, address );
    }

    switch ( msg->getBaseType().value )
    {
        case BaseType::SerializableMessage:
            gbn.delayKeepAliveOnce();
            return sendRaw ( msg, address );

        case BaseType::SerializableSequence:
            gbn.sendGoBackN ( msg );
            return isConnected();

        default:
            LOG ( "Unhandled BaseType '%s'!", msg->getBaseType() );
            return false;
    }
}

bool UdpSocket::sendRaw ( const MsgPtr& msg, const IpAddrPort& address )
{
#ifndef RELEASE
    // Simulate failed checksum
    if ( checkSumFail && msg )
    {
        if ( rand() % 100 < checkSumFail )
        {
            LOG ( "Munging checksum for '%s'", msg );
            for ( char& byte : msg->md5 )
                byte = ( rand() % 0x100 );
            msg->md5empty = false;
        }
        else
        {
            msg->md5empty = true;
        }
    }
#endif

    string buffer = ::Protocol::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() && buffer.size() <= 256 )
        LOG ( "Base64 : %s", toBase64 ( buffer ) );

    // Real UDP sockets send directly
    if ( isReal()  )
        return Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    // Child UDP sockets send via parent if not disconnected
    if ( isChild() && parentSocket )
        return parentSocket->Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    LOG_UDP_SOCKET ( this, "Cannot send over disconnected socket" );
    return false;
}

void UdpSocket::sendRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &this->gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    sendRaw ( msg, getRemoteAddress() );
}

void UdpSocket::recvRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &this->gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    if ( owner )
        owner->readEvent ( this, msg, getRemoteAddress() );
}

void UdpSocket::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    ASSERT ( gbn == &this->gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    // Child sockets handle GoBackN messages as a proxy of the parent socket,
    // this is so the GoBackN state resides in the child socket.
    if ( isChild() )
    {
        ASSERT ( parentSocket->childSockets.find ( getRemoteAddress() ) != parentSocket->childSockets.end() );
        ASSERT ( parentSocket->childSockets[getRemoteAddress()].get() == this );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                if ( isConnecting() )
                {
                    switch ( msg->getAs<UdpConnect>().value )
                    {
                        case UdpConnect::Request:
                            // UdpConnect::Request should be responded to with a Reply
                            send ( new UdpConnect ( UdpConnect::Reply ) );
                            break;

                        case UdpConnect::Final:
                            // UdpConnect::Final means the client connected properly and is now accepted
                            state = State::Connected;

                            LOG_UDP_SOCKET ( this, "acceptEvent" );

                            parentSocket->acceptedSocket = parentSocket->childSockets[getRemoteAddress()];

                            gbn->setKeepAlive ( keepAlive );

                            if ( parentSocket->owner )
                                parentSocket->owner->acceptEvent ( parentSocket );
                            break;

                        default:
                            break;
                    }
                    return;
                }

                LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
                break;

            default:
                if ( owner )
                    owner->readEvent ( this, msg, getRemoteAddress() );
                break;
        }
    }
    else
    {
        ASSERT ( isReal() == true );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                // UdpConnect::Reply while connecting indicates this socket is now connected
                if ( isConnecting() && msg->getAs<UdpConnect>().value == UdpConnect::Reply )
                {
                    state = State::Connected;

                    LOG_UDP_SOCKET ( this, "connectEvent" );

                    send ( new UdpConnect ( UdpConnect::Final ) );

                    gbn->setKeepAlive ( keepAlive );

                    if ( owner )
                        owner->connectEvent ( this );
                    return;
                }

                LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
                break;

            default:
                if ( owner )
                    owner->readEvent ( this, msg, getRemoteAddress() );
                break;
        }
    }
}

void UdpSocket::timeoutGoBackN ( GoBackN *gbn )
{
    ASSERT ( gbn == &this->gbn );
    ASSERT ( getRemoteAddress().empty() == false );

    LOG_UDP_SOCKET ( this, "disconnectEvent" );

    Socket::Owner *const owner = this->owner;

    disconnect();

    if ( owner )
        owner->disconnectEvent ( this );
}

void UdpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( isConnectionLess() )
    {
        // Recv directly if we're in connection-less mode
        if ( owner )
            owner->readEvent ( this, msg, address );
    }
    else if ( isClient() )
    {
        // Client UDP sockets recv into the GoBackN instance
        gbn.recvRaw ( msg );
    }
    else if ( isServer() )
    {
        // Server UDP sockets recv into the addressed child socket
        readEventAddressed ( msg, address );
    }
    else
    {
        LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
    }
}

void UdpSocket::readEventAddressed ( const MsgPtr& msg, const IpAddrPort& address )
{
    UdpSocket *socket;

    auto it = childSockets.find ( address );
    if ( it != childSockets.end() )
    {
        // Get the existing child socket
        socket = & ( it->second->getAsUDP() );
    }
    else if ( msg.get()
              && msg->getMsgType() == MsgType::UdpConnect
              && msg->getAs<UdpConnect>().value == UdpConnect::Request )
    {
        // Only a connect request is allowed to open a new child socket
        socket = new UdpSocket ( ChildSocket, this, address );
        childSockets.insert ( make_pair ( address, SocketPtr ( socket ) ) );
    }
    else
    {
        LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
        LOG_UDP_SOCKET ( this, "Ignoring because no child socket for '%s'", address );
        return;
    }

    ASSERT ( socket != 0 );

    socket->readEvent ( msg, address );
}

MsgPtr UdpSocket::share ( int processId )
{
    if ( isChild() )
        return NullMsg;

    MsgPtr data = Socket::share ( processId );

    ASSERT ( typeid ( *data ) == typeid ( SocketShareData ) );
    ASSERT ( isReal() == true );

    LOG ( "Sharing UDP:" );

    switch ( type.value )
    {
        case Type::ConnectionLess:
            LOG ( "connection-less: address='%s'", address );
            break;

        case Type::Client:
            LOG ( "client: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( gbn ) );
            break;

        case Type::Server:
            LOG ( "server: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( gbn ) );

            for ( const auto& kv : childSockets )
            {
                LOG ( "child: address='%s'; keepAlive=%d", kv.first, kv.second->getAsUDP().getKeepAlive() );
                kv.second->getAsUDP().gbn.logSendList();

                data->getAs<SocketShareData>().childSockets[kv.first] = kv.second->getAsUDP().gbn;
                kv.second->getAsUDP().gbn.reset(); // Reset to stop the GoBackN timers from firing
            }
            break;

        default:
            return NullMsg;
    }

    data->getAs<SocketShareData>().udpType = type.value;
    gbn.reset(); // Reset to stop the GoBackN timers from firing

    return data;
}

void UdpSocket::listen()
{
    if ( type == Type::Server )
        return;

    resetBuffer();

    ASSERT ( address.addr.empty() == true );
    ASSERT ( parentSocket == 0 );
    ASSERT ( childSockets.empty() == true );
    ASSERT ( acceptedSocket.get() == 0 );

    isRaw = false;
    type = Type::Server;
    gbn.setSendInterval ( DEFAULT_SEND_INTERVAL );
    gbn.setKeepAlive ( DEFAULT_KEEP_ALIVE );
    gbn.reset();
}

void UdpSocket::connect()
{
    if ( type == Type::Client )
        return;

    resetBuffer();

    ASSERT ( address.addr.empty() == false );
    ASSERT ( parentSocket == 0 );
    ASSERT ( childSockets.empty() == true );
    ASSERT ( acceptedSocket.get() == 0 );

    isRaw = false;
    type = Type::Client;
    state = State::Connecting;
    gbn.setSendInterval ( DEFAULT_SEND_INTERVAL );
    gbn.setKeepAlive ( DEFAULT_KEEP_ALIVE );
    gbn.reset();

    send ( new UdpConnect ( UdpConnect::Request ) );
}
