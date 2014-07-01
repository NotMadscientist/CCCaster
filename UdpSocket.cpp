// #include "UdpSocket.h"
// #include "Log.h"
// #include "Util.h"
//
// #include <cassert>
// #include <typeinfo>
//
// using namespace std;
//
// #define KEEP_ALIVE 2000
//
// #define LOG_SOCKET(VERB, SOCKET)                                                                            \
//     LOG ( "%s UDP socket %08x; parent=%08x; owner=%08x; address='%s'",                                      \
//           VERB, SOCKET, SOCKET->parentSocket, SOCKET->proxiedOwner, SOCKET->address.c_str() )
//
// void UdpSocket::sendGoBackN ( GoBackN *gbn, const MsgPtr& msg )
// {
//     assert ( gbn == &this->gbn );
//     assert ( !getRemoteAddress().empty() );
//
//     if ( state == State::Disconnected )
//         return;
//
//     if ( parentSocket == 0 )
//         Socket::send ( msg, getRemoteAddress() );
//     else
//         parentSocket->Socket::send ( msg, getRemoteAddress() );
// }
//
// void UdpSocket::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
// {
//     assert ( gbn == &this->gbn );
//     assert ( !getRemoteAddress().empty() );
//
//     if ( state == State::Disconnected )
//         return;
//
//     LOG_SOCKET ( TO_C_STR ( "Got '%s' from", TO_C_STR ( msg ) ), this );
//
//     if ( parentSocket == 0 )
//     {
//         switch ( msg->getMsgType() )
//         {
//             case MsgType::UdpConnect:
//                 if ( msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Reply )
//                 {
//                     LOG_SOCKET ( "Connected", this );
//                     send ( new UdpConnect ( UdpConnect::ConnectType::Final ) );
//                     state = State::Connected;
//                     proxiedOwner->connectEvent ( this );
//                 }
//                 break;
//
//             default:
//                 proxiedOwner->readEvent ( this, msg, getRemoteAddress() );
//                 break;
//         }
//     }
//     else
//     {
//         assert ( parentSocket->acceptedSockets.find ( getRemoteAddress() ) != parentSocket->acceptedSockets.end() );
//
//         switch ( msg->getMsgType() )
//         {
//             case MsgType::UdpConnect:
//                 switch ( msg->getAs<UdpConnect>().connectType )
//                 {
//                     case UdpConnect::ConnectType::Request:
//                         parentSocket->acceptedSockets[getRemoteAddress()]->send (
//                             new UdpConnect ( UdpConnect::ConnectType::Reply ) );
//                         break;
//
//                     case UdpConnect::ConnectType::Final:
//                         LOG_SOCKET ( "Accept from server", parentSocket );
//                         parentSocket->acceptedSocket = parentSocket->acceptedSockets[getRemoteAddress()];
//                         parentSocket->proxiedOwner->acceptEvent ( parentSocket );
//                         break;
//
//                     default:
//                         break;
//                 }
//                 break;
//
//             default:
//                 proxiedOwner->readEvent ( this, msg, getRemoteAddress() );
//                 break;
//         }
//     }
// }
//
// void UdpSocket::timeoutGoBackN ( GoBackN *gbn )
// {
//     assert ( gbn == &this->gbn );
//     assert ( !getRemoteAddress().empty() );
//
//     LOG_SOCKET ( "Disconnected", this );
//     Socket::Owner *owner = ( parentSocket == 0 ? proxiedOwner : parentSocket->proxiedOwner );
//     disconnect();
//     owner->disconnectEvent ( this );
// }
//
// void UdpSocket::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
// {
//     assert ( owner == this );
//     assert ( socket == this );
//
//     if ( isClient() )
//         gbn.recv ( msg );
//     else
//         gbnRecvAddressed ( msg, address );
// }
//
// void UdpSocket::gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address )
// {
//     UdpSocket *socket;
//
//     auto it = acceptedSockets.find ( address );
//     if ( it != acceptedSockets.end() )
//     {
//         assert ( typeid ( *it->second ) == typeid ( UdpSocket ) );
//         socket = static_cast<UdpSocket *> ( it->second.get() );
//
//         if ( socket->state == State::Disconnected )
//             return;
//     }
//     else if ( msg.get() && msg->getMsgType() == MsgType::UdpConnect
//               && msg->getAs<UdpConnect>().connectType == UdpConnect::ConnectType::Request )
//     {
//         // Only a connect request is allowed to open a new accepted socket
//         socket = new UdpSocket ( this, 0, address.addr, address.port );
//         acceptedSockets.insert ( make_pair ( address, shared_ptr<Socket> ( socket ) ) );
//     }
//     else
//     {
//         return;
//     }
//
//     socket->gbn.recv ( msg );
// }
//
// UdpSocket::UdpSocket ( Socket::Owner *owner, unsigned port )
//     : Socket ( this, port, Protocol::UDP ), state ( State::Listening )
//     , parentSocket ( 0 ), proxiedOwner ( owner ), gbn ( this, KEEP_ALIVE )
// {
//     LOG_SOCKET ( "Listening to server", this );
// }
//
// UdpSocket::UdpSocket ( Socket::Owner *owner, const string& address, unsigned port )
//     : Socket ( this, address, port, Protocol::UDP ), state ( State::Connecting )
//     , parentSocket ( 0 ), proxiedOwner ( owner ), gbn ( this, KEEP_ALIVE )
// {
//     LOG_SOCKET ( "Connecting", this );
//     send ( new UdpConnect ( UdpConnect::ConnectType::Request ) );
// }
//
// UdpSocket::UdpSocket ( UdpSocket *parent, Socket::Owner *owner, const string& address, unsigned port )
//     : Socket ( this, address, port ), state ( State::Connected )
//     , parentSocket ( parent ), proxiedOwner ( owner ), gbn ( this, parent->getKeepAlive() )
// {
//     LOG_SOCKET ( "Pending", this );
// }
//
// UdpSocket::~UdpSocket()
// {
//     disconnect();
// }
//
// void UdpSocket::disconnect()
// {
//     LOG_SOCKET ( "Disconnect", this );
//
//     if ( parentSocket == 0 )
//         Socket::disconnect();
//
//     state = State::Disconnected;
//
//     gbn.reset();
//     gbn.setKeepAlive ( 0 );
//
//     for ( auto& kv : acceptedSockets )
//     {
//         assert ( typeid ( *kv.second ) == typeid ( UdpSocket ) );
//         assert ( static_cast<UdpSocket *> ( kv.second.get() )->parentSocket == this );
//         static_cast<UdpSocket *> ( kv.second.get() )->parentSocket = 0;
//     }
//
//     if ( parentSocket != 0 )
//         parentSocket->acceptedSockets.erase ( getRemoteAddress() );
// }
//
// shared_ptr<Socket> UdpSocket::listen ( Socket::Owner *owner, unsigned port )
// {
//     return shared_ptr<Socket> ( new UdpSocket ( owner, port ) );
// }
//
// shared_ptr<Socket> UdpSocket::connect ( Socket::Owner *owner, const string& address, unsigned port )
// {
//     return shared_ptr<Socket> ( new UdpSocket ( owner, address, port ) );
// }
//
// shared_ptr<Socket> UdpSocket::accept ( Socket::Owner *owner )
// {
//     if ( !acceptedSocket.get() )
//         return 0;
//
//     acceptedSocket->setOwner ( owner );
//
//     shared_ptr<Socket> ret;
//     acceptedSocket.swap ( ret );
//     return ret;
// }
//
// void UdpSocket::send ( Serializable *message, const IpAddrPort& address )
// {
//     if ( state == State::Disconnected )
//         return;
//
//     MsgPtr msg ( message );
//     send ( msg, address );
// }
//
// void UdpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
// {
//     if ( state == State::Disconnected )
//         return;
//
//     switch ( msg->getBaseType() )
//     {
//         case BaseType::SerializableMessage:
//             Socket::send ( msg, address );
//             break;
//
//         case BaseType::SerializableSequence:
//             assert ( getRemoteAddress().empty() == false );
//             gbn.send ( msg );
//             break;
//
//         default:
//             LOG ( "Unhandled BaseType '%s'!", TO_C_STR ( msg->getBaseType() ) );
//             break;
//     }
// }
