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


#define DEFAULT_KEEP_ALIVE 1000


UdpSocket::UdpSocket ( Socket::Owner *owner, uint16_t port, uint64_t keepAlive  )
    : Socket ( IpAddrPort ( "", port ), Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    SocketManager::get().add ( this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive )
    : Socket ( address, Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = ( keepAlive ? State::Connecting : State::Connected );
    Socket::init();
    SocketManager::get().add ( this );

    if ( isConnecting() )
        send ( new UdpConnect ( UdpConnect::ConnectType::Request ) );
}

UdpSocket::UdpSocket ( ChildSocketEnum, UdpSocket *parent, const IpAddrPort& address )
    : Socket ( address, Protocol::UDP ), hasParent ( true ), parent ( parent ), gbn ( this, parent->getKeepAlive() )
{
    this->state = State::Connected;
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const SocketShareData& data )
    : Socket ( data.address, Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, 0 )
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
        LOG_SOCKET ( this, "%s; WSASocket failed", err );
        this->fd = 0;
        throw err;
    }

    SocketManager::get().add ( this );
}

UdpSocket::~UdpSocket()
{
    disconnect();
}

void UdpSocket::disconnect()
{
    if ( !isChild() )
        SocketManager::get().remove ( this );

    Socket::disconnect();
    gbn.reset();
    gbn.setKeepAlive ( 0 );

    // Detach child sockets first
    for ( auto& kv : childSockets )
    {
        assert ( typeid ( *kv.second ) == typeid ( UdpSocket ) );
        static_cast<UdpSocket *> ( kv.second.get() )->parent = 0;
    }

    // Check and remove child from parent
    if ( parent != 0 )
    {
        parent->childSockets.erase ( getRemoteAddress() );
        parent = 0;
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

    switch ( msg->getBaseType() )
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
    string buffer = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() )
        LOG ( "Base64 : %s", toBase64 ( &buffer[0], min ( 256u, buffer.size() ) ) );

    if ( !isChild() )
        return Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    if ( parent )
        return parent->Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    LOG_SOCKET ( this, "Cannot send over disconnected socket" );
    return false;
}

void UdpSocket::sendRaw ( GoBackN *gbn, const MsgPtr& msg )
{
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    sendRaw ( msg, getRemoteAddress() );
}

void UdpSocket::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    LOG_SOCKET ( this, "got '%s'", msg );

    if ( !isChild() )
    {
        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                if ( msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Reply )
                {
                    LOG_SOCKET ( this, "connectEvent" );
                    send ( new UdpConnect ( UdpConnect::ConnectType::Final ) );
                    state = State::Connected;
                    if ( owner )
                        owner->connectEvent ( this );
                }
                break;

            default:
                if ( owner )
                    owner->readEvent ( this, msg, getRemoteAddress() );
                break;
        }
    }
    else
    {
        assert ( parent->childSockets.find ( getRemoteAddress() ) != parent->childSockets.end() );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                switch ( msg->getAs<UdpConnect>().connectType )
                {
                    case UdpConnect::ConnectType::Request:
                        parent->childSockets[getRemoteAddress()]->send (
                            new UdpConnect ( UdpConnect::ConnectType::Reply ) );
                        break;

                    case UdpConnect::ConnectType::Final:
                        LOG_SOCKET ( this, "acceptEvent" );
                        parent->acceptedSocket = parent->childSockets[getRemoteAddress()];
                        if ( parent->owner )
                            parent->owner->acceptEvent ( parent );
                        break;

                    default:
                        break;
                }
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

    LOG_SOCKET ( this, "disconnectEvent" );
    Socket::Owner *owner = this->owner;
    disconnect();
    if ( owner )
        owner->disconnectEvent ( this );
}

void UdpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    assert ( isChild() == false );

    if ( getKeepAlive() == 0 )
    {
        if ( owner )
            owner->readEvent ( this, msg, address );
    }
    else if ( isClient() )
    {
        gbn.recvRaw ( msg );
    }
    else
    {
        gbnRecvAddressed ( msg, address );
    }
}

void UdpSocket::gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address )
{
    UdpSocket *socket;

    auto it = childSockets.find ( address );
    if ( it != childSockets.end() )
    {
        // Get the existing child socket
        assert ( typeid ( *it->second ) == typeid ( UdpSocket ) );
        socket = static_cast<UdpSocket *> ( it->second.get() );
    }
    else if ( msg.get() && msg->getMsgType() == MsgType::UdpConnect
              && msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Request )
    {
        // Only a connect request is allowed to open a new child socket
        socket = new UdpSocket ( ChildSocket, this, address );
        childSockets.insert ( make_pair ( address, SocketPtr ( socket ) ) );
    }
    else
    {
        return;
    }

    socket->gbn.recvRaw ( msg );
}
