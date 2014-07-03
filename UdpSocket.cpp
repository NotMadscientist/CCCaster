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

    LOG_SOCKET ( ( "Got '" + toString ( msg ) + "' from" ).c_str(), this );

    if ( isChild() )
    {
        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                if ( msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Reply )
                {
                    LOG_SOCKET ( "Connected", this );
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
        assert ( parent->acceptedSockets.find ( getRemoteAddress() ) != parent->acceptedSockets.end() );

        switch ( msg->getMsgType() )
        {
            case MsgType::UdpConnect:
                switch ( msg->getAs<UdpConnect>().connectType )
                {
                    case UdpConnect::ConnectType::Request:
                        parent->acceptedSockets[getRemoteAddress()]->send (
                            new UdpConnect ( UdpConnect::ConnectType::Reply ) );
                        break;

                    case UdpConnect::ConnectType::Final:
                        LOG_SOCKET ( "Accept from server", parent );
                        parent->acceptedSocket = parent->acceptedSockets[getRemoteAddress()];
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

    LOG_SOCKET ( "Disconnected", this );
    Socket::Owner *owner = this->owner;
    disconnect();
    if ( owner )
        owner->disconnectEvent ( this );
}

void UdpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
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

    auto it = acceptedSockets.find ( address );
    if ( it != acceptedSockets.end() )
    {
        assert ( typeid ( *it->second ) == typeid ( UdpSocket ) );
        socket = static_cast<UdpSocket *> ( it->second.get() );
    }
    else if ( msg.get() && msg->getMsgType() == MsgType::UdpConnect
              && msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Request )
    {
        // Only a connect request is allowed to open a new accepted socket
        socket = new UdpSocket ( this, address );
        acceptedSockets.insert ( make_pair ( address, shared_ptr<Socket> ( socket ) ) );
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
    LOG_SOCKET ( "Listening to server", this );
}

UdpSocket::UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive )
    : Socket ( address, Protocol::UDP ), hasParent ( false ), parent ( 0 ), gbn ( this, keepAlive )
{
    this->owner = owner;
    this->state = ( keepAlive ? State::Connecting : State::Connected );
    Socket::init();
    EventManager::get().addSocket ( this );
    LOG_SOCKET ( TO_C_STR ( state ), this );

    if ( state == State::Connecting )
        send ( new UdpConnect ( UdpConnect::ConnectType::Request ) );
}

UdpSocket::UdpSocket ( UdpSocket *parent, const IpAddrPort& address )
    : Socket ( address, Protocol::UDP ), hasParent ( true ), parent ( parent ), gbn ( this, parent->getKeepAlive() )
{
    this->state = State::Connected;
    LOG_SOCKET ( "Pending", this );
}

UdpSocket::~UdpSocket()
{
    disconnect();
}

void UdpSocket::disconnect()
{
    if ( !isChild() )
    {
        EventManager::get().removeSocket ( this );
        Socket::disconnect();
    }

    state = State::Disconnected;

    gbn.reset();
    gbn.setKeepAlive ( 0 );

    for ( auto& kv : acceptedSockets )
    {
        assert ( typeid ( *kv.second ) == typeid ( UdpSocket ) );
        assert ( static_cast<UdpSocket *> ( kv.second.get() )->parent == this );
        static_cast<UdpSocket *> ( kv.second.get() )->parent = 0;
    }

    if ( parent != 0 )
    {
        parent->acceptedSockets.erase ( getRemoteAddress() );
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
            LOG ( "Unhandled BaseType '%s'!", TO_C_STR ( msg->getBaseType() ) );
            return false;
    }
}

bool UdpSocket::sendDirect ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( !isDisconnected() )
    {
        string buffer = Serializable::encode ( msg );

        LOG ( "Encoded '%s' to [ %u bytes ]", TO_C_STR ( msg ), buffer.size() );

        if ( !buffer.empty() )
            LOG ( "Base64 : %s", toBase64 ( buffer ).c_str() );

        if ( isChild() )
            return Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );

        if ( parent )
            return parent->Socket::send ( &buffer[0], buffer.size(), address.empty() ? this->address : address );
    }

    LOG_SOCKET ( "Cannot send over disconnected socket", this );
    return false;
}
