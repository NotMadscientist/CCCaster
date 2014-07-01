#include "Test.Socket.h"
#include "UdpSocket.h"
#include "Timer.h"

#include <memory>

using namespace std;

// #define PACKET_LOSS     90
// #define KEEP_ALIVE      ( 30 * 1000 )
//
// TEST_CONNECT                ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 60 * 1000 )
//
// TEST_TIMEOUT                ( UdpSocket, Udp, 0, 1000, 1000 )
//
// TEST_CLIENT_DISCONNECT      ( UdpSocket, Udp, 0, 1000, 3000 )
//
// TEST_SERVER_DISCONNECT      ( UdpSocket, Udp, 0, 1000, 5000 )
//
// TEST_SEND                   ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )
//
// // This test doesn't make sense since there is only one UDP socket
// // TEST_SEND_WITHOUT_SERVER    ( UdpSocket, Udp, PACKET_LOSS, KEEP_ALIVE, 120 * 1000 )

TEST ( UdpSocket, SendConnectionLess )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket;
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

        TestSocket ( unsigned port )
            : socket ( UdpSocket::bind ( this, port ) )
            , timer ( this ), sent ( false )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , timer ( this ), sent ( false )
        {
            timer.start ( 1000 );
        }
    };

    TestSocket server ( TEST_LOCAL_PORT );
    TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket.get() );
    if ( server.socket.get() )
        EXPECT_TRUE ( server.socket->isServer() );

    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->getMsgType(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->getMsgType(), MsgType::TestMessage );
        EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );
    }
}
