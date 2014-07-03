#include "Event.h"
#include "UdpSocket.h"
#include "Log.h"
#include "Util.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>
#include <typeinfo>

#define DEFAULT_KEEP_ALIVE 2000

using namespace std;

void UdpSocket::sendGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    assert ( gbn == &this->gbn );
    assert ( getRemoteAddress().empty() == false );

    sendDirect ( msg, getRemoteAddress() );
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
        gbn.recv ( msg );
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
        socket = new UdpSocket ( this, address );
        childSockets.insert ( make_pair ( address, shared_ptr<Socket> ( socket ) ) );
    }
    else
    {
        return;
    }

    socket->gbn.recv ( msg );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, uint16_t port, uint64_t keepAlive  )
    : Socket ( IpAddrPort ( "", port ), Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    EventManager::get().addSocket ( this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive )
    : Socket ( address, Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = ( keepAlive ? State::Connecting : State::Connected );
    Socket::init();
    EventManager::get().addSocket ( this );

    if ( isConnecting() )
        send ( new UdpConnect ( UdpConnect::ConnectType::Request ) );
}

UdpSocket::UdpSocket ( UdpSocket *parent, const IpAddrPort& address )
    : Socket ( address, Protocol::UDP ), hasParent ( true ), parent ( parent ), gbn ( this, parent->getKeepAlive() )
{
    this->state = State::Connected;
}

UdpSocket::~UdpSocket()
{
    disconnect();
}

void UdpSocket::disconnect()
{
    if ( !isChild() )
        EventManager::get().removeSocket ( this );

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

shared_ptr<Socket> UdpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return shared_ptr<Socket> ( new UdpSocket ( owner, port, DEFAULT_KEEP_ALIVE ) );
}

shared_ptr<Socket> UdpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return shared_ptr<Socket> ( new UdpSocket ( owner, address, DEFAULT_KEEP_ALIVE ) );
}

shared_ptr<Socket> UdpSocket::bind ( Socket::Owner *owner, uint16_t port )
{
    return shared_ptr<Socket> ( new UdpSocket ( owner, port, 0 ) );
}

shared_ptr<Socket> UdpSocket::bind ( Socket::Owner *owner, const IpAddrPort& address )
{
    return shared_ptr<Socket> ( new UdpSocket ( owner, address, 0 ) );
}

shared_ptr<Socket> UdpSocket::accept ( Socket::Owner *owner )
{
    if ( !acceptedSocket.get() )
        return 0;

    acceptedSocket->owner = owner;

    shared_ptr<Socket> ret;
    acceptedSocket.swap ( ret );
    return ret;
}

bool UdpSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    return sendDirect ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ), address );
}

bool UdpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( getKeepAlive() == 0 || !msg.get() )
        return sendDirect ( msg, address );

    switch ( msg->getBaseType() )
    {
        case BaseType::SerializableMessage:
            return sendDirect ( msg, address );

        case BaseType::SerializableSequence:
            gbn.send ( msg );
            return true;

        default:
            LOG ( "Unhandled BaseType '%s'!", msg->getBaseType() );
            return false;
    }
}

bool UdpSocket::sendDirect ( const MsgPtr& msg, const IpAddrPort& address )
{
    string buffer = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() )
        LOG ( "Base64 : %s", toBase64 ( buffer ) );

    if ( !isChild() )
        return Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    if ( parent )
        return parent->Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

    LOG_SOCKET ( this, "Cannot send over disconnected socket" );
    return false;
}
