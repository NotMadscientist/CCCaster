#include "Test.Socket.h"
#include "UdpSocket.h"
#include "Timer.h"

#include <memory>

using namespace std;

#define PACKET_LOSS     50
#define LONG_TIMEOUT    ( 120 * 1000 )

TEST_CONNECT                ( UdpSocket, PACKET_LOSS, LONG_TIMEOUT, LONG_TIMEOUT )

TEST_TIMEOUT                ( UdpSocket, 0, 1000, 1000 )

TEST_DISCONNECT_CLIENT      ( UdpSocket, 0, 1000, LONG_TIMEOUT )

TEST_DISCONNECT_ACCEPTED    ( UdpSocket, 0, 1000, LONG_TIMEOUT )

TEST_SEND                   ( UdpSocket, PACKET_LOSS, LONG_TIMEOUT, LONG_TIMEOUT )

// This test doesn't make sense since there is only one UDP socket
// TEST_SEND_WITHOUT_SERVER    ( UdpSocket, Udp, PACKET_LOSS, LONG_TIMEOUT, LONG_TIMEOUT )

TEST ( UdpSocket, SendConnectionLess )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        SocketPtr socket;
        Timer timer;
        MsgPtr msg;
        bool sent;

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            this->msg = msg;

            if ( socket->getRemoteAddress().addr.empty() )
            {
                socket->send ( new TestMessage ( "Hello client!" ), address );
                sent = true;
            }
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( !sent )
            {
                if ( !socket->getRemoteAddress().addr.empty() )
                {
                    socket->send ( new TestMessage ( "Hello server!" ) );
                    sent = true;
                }

                timer->start ( 1000 );
            }
            else
            {
                EventManager::get().stop();
            }
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) )
            , timer ( this ), sent ( false )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , timer ( this ), sent ( false )
        {
            timer.start ( 1000 );
        }
    };

    TimerManager::get().initialize();
    SocketManager::get().initialize();

    TestSocket server ( 0 );
    TestSocket client ( "127.0.0.1", server.socket->address.port );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket.get() );
    if ( server.socket.get() )
        EXPECT_TRUE ( server.socket->isServer() );

    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, server.msg->getMsgType() );
        EXPECT_EQ ( "Hello server!", server.msg->getAs<TestMessage>().str );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, client.msg->getMsgType() );
        EXPECT_EQ ( "Hello client!", client.msg->getAs<TestMessage>().str );
    }

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}
