#include "SocketManager.h"
#include "UdpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>
#include <typeinfo>
#include <algorithm>

using namespace std;


#define DEFAULT_KEEP_ALIVE 2000

#define LOG_UDP_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, "type=%s; " FORMAT, type, ## __VA_ARGS__)


UdpSocket::UdpSocket ( Socket::Owner *owner, uint16_t port, uint64_t keepAlive )
    : Socket ( IpAddrPort ( "", port ), Protocol::UDP )
    , type ( Type::Server )
    , gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive )
    : Socket ( address, Protocol::UDP )
    , type ( Type::Client )
    , gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = ( keepAlive ? State::Connecting : State::Connected );
    Socket::init();
    SocketManager::get().add ( this );

    if ( isConnecting() )
        send ( new UdpConnect ( UdpConnect::Request ) );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( data.address, Protocol::UDP )
    , type ( data.isUdpServer ? Type::Server : Type::Client )
    , gbn ( this, 0 )
{
    assert ( data.protocol == Protocol::UDP );

    this->owner = owner;
    this->state = data.state;
    this->readBuffer = data.readBuffer;
    this->readPos = data.readPos;

    assert ( data.info->iSocketType == SOCK_DGRAM );
    assert ( data.info->iProtocol == IPPROTO_UDP );

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

                LOG ( "childSocket: address='%s'; keepAlive=%d", socket->address, socket->getKeepAlive() );
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
    : Socket ( address, Protocol::UDP )
    , type ( Type::Child )
    , gbn ( this, parentSocket->getKeepAlive() )
    , parentSocket ( parentSocket )
{
    this->state = State::Connecting;
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address, const GoBackN& state )
    : Socket ( address, Protocol::UDP )
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
    return SocketPtr ( new UdpSocket ( owner, port, DEFAULT_KEEP_ALIVE ) );
}

SocketPtr UdpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return SocketPtr ( new UdpSocket ( owner, address, DEFAULT_KEEP_ALIVE ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, uint16_t port )
{
    return SocketPtr ( new UdpSocket ( owner, port, 0 ) );
}

SocketPtr UdpSocket::bind ( Socket::Owner *owner, const IpAddrPort& address )
{
    return SocketPtr ( new UdpSocket ( owner, address, 0 ) );
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
    return sendRaw ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( getKeepAlive() == 0 || !msg.get() )
        return sendRaw ( msg, address );

    switch ( msg->getBaseType().value )
    {
        case BaseType::SerializableMessage:
            return sendRaw ( msg, address );

        case BaseType::SerializableSequence:
            gbn.sendGoBackN ( msg );
            return true;

        default:
            LOG ( "Unhandled BaseType '%s'!", msg->getBaseType() );
            return false;
    }
}

bool UdpSocket::sendRaw ( const MsgPtr& msg, const IpAddrPort& address )
{
#ifndef RELEASE
    // Simulate failed checksum
    if ( rand() % 100 < checkSumFail && msg )
    {
        LOG ( "Munging checksum for '%s'", msg );
        for ( char& byte : msg->md5 )
            byte = ( rand() % 0x100 );
        msg->md5empty = false;
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
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    sendRaw ( msg, getRemoteAddress() );
}

void UdpSocket::recvRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    if ( owner )
        owner->readEvent ( this, msg, getRemoteAddress() );
}

void UdpSocket::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    // Child sockets handle GoBackN message as a proxy of the parent socket,
    // this is so the GoBackN state resides in the child socket.
    if ( isChild() )
    {
        assert ( parentSocket->childSockets.find ( getRemoteAddress() ) != parentSocket->childSockets.end() );
        assert ( parentSocket->childSockets[getRemoteAddress()].get() == this );

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
                            // UdpConnect::Final message means the client connected properly and is now accepted
                            state = State::Connected;
                            LOG_UDP_SOCKET ( this, "acceptEvent" );
                            parentSocket->acceptedSocket = parentSocket->childSockets[getRemoteAddress()];
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
        assert ( isReal() == true );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                if ( isConnecting() && msg->getAs<UdpConnect>().value == UdpConnect::Reply )
                {
                    // UdpConnect::Reply while connecting indicates this socket is now connected
                    state = State::Connected;
                    send ( new UdpConnect ( UdpConnect::Final ) );
                    LOG_UDP_SOCKET ( this, "connectEvent" );
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
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    LOG_UDP_SOCKET ( this, "disconnectEvent" );

    Socket::Owner *owner = this->owner;

    disconnect();

    if ( owner )
        owner->disconnectEvent ( this );
}

void UdpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( getKeepAlive() == 0 )
    {
        // Recv directly if not in GoBackN mode
        if ( owner )
            owner->readEvent ( this, msg, address );
        return;
    }

    if ( isClient() )
    {
        // Client UDP sockets recv into the GoBackN instance
        gbn.recvRaw ( msg );
        return;
    }

    if ( isServer() )
    {
        // Server UDP sockets recv into the addressed socket
        gbnRecvAddressed ( msg, address );
        return;
    }

    LOG_UDP_SOCKET ( this, "Unexpected '%s' from '%s'", msg, address );
}

void UdpSocket::gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address )
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

    assert ( socket != 0 );

    socket->readEvent ( msg, address );
}

MsgPtr UdpSocket::share ( int processId )
{
    if ( isChild() )
        return NullMsg;

    MsgPtr data = Socket::share ( processId );

    assert ( typeid ( *data ) == typeid ( SocketShareData ) );
    assert ( type == Type::Client || type == Type::Server );

    LOG ( "Sharing UDP:" );

    switch ( type.value )
    {
        case Type::Client:
            data->getAs<SocketShareData>().isUdpServer = false;

            LOG ( "client: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( gbn ) );
            gbn.reset(); // Reset to stop the GoBackN timers from firing
            break;

        case Type::Server:
            data->getAs<SocketShareData>().isUdpServer = true;

            LOG ( "server: address='%s'; keepAlive=%d", address, getKeepAlive() );
            gbn.logSendList();

            data->getAs<SocketShareData>().gbnState.reset ( new GoBackN ( gbn ) );
            gbn.reset(); // Reset to stop the GoBackN timers from firing

            for ( const auto& kv : childSockets )
            {
                LOG ( "childSocket: address='%s'; keepAlive=%d", kv.first, kv.second->getAsUDP().getKeepAlive() );
                kv.second->getAsUDP().gbn.logSendList();

                data->getAs<SocketShareData>().childSockets[kv.first] = kv.second->getAsUDP().gbn;
                kv.second->getAsUDP().gbn.reset(); // Reset to stop the GoBackN timers from firing
            }
            break;

        default:
            return NullMsg;
    }

    return data;
}
