#ifndef RELEASE

#include "Test.Socket.h"
#include "UdpSocket.h"
#include "Timer.h"

#include <memory>

using namespace std;


#define PACKET_LOSS     50
#define CHECK_SUM_FAIL  50
#define LONG_TIMEOUT    ( 120 * 1000 )


TEST_CONNECT                ( UdpSocket, PACKET_LOSS, CHECK_SUM_FAIL, LONG_TIMEOUT, LONG_TIMEOUT )

TEST_TIMEOUT                ( UdpSocket, 0, 0, 1000, 1000 )

TEST_DISCONNECT_CLIENT      ( UdpSocket, 0, 0, 1000, LONG_TIMEOUT )

TEST_DISCONNECT_ACCEPTED    ( UdpSocket, 0, 0, 1000, LONG_TIMEOUT )

TEST_SEND                   ( UdpSocket, PACKET_LOSS, CHECK_SUM_FAIL, LONG_TIMEOUT, LONG_TIMEOUT )

// This test doesn't make sense since there is only one UDP socket
// TEST_SEND_WITHOUT_SERVER    ( UdpSocket, PACKET_LOSS, CHECK_SUM_FAIL, LONG_TIMEOUT, LONG_TIMEOUT )


TEST ( UdpSocket, SendConnectionLess )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        SocketPtr socket;
        Timer timer;
        MsgPtr msg;
        bool sent;

        void acceptEvent ( Socket *socket ) override {}
        void connectEvent ( Socket *socket ) override {}
        void disconnectEvent ( Socket *socket ) override {}

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

TEST ( UdpSocket, BindThenConnect )
{
    static int done = 0;
    done = 0;

    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        SocketPtr socket, accepted;
        Timer timer;
        MsgPtr bindedMsg;
        MsgPtr connectedMsg;
        int step = 0;

        void acceptEvent ( Socket *socket ) override
        {
            accepted = socket->accept ( this );
            accepted->send ( new TestMessage ( "Accepted message" ), accepted->address );
            ++done;
        }

        void connectEvent ( Socket *socket ) override
        {
            socket->send ( new TestMessage ( "Connected message" ), socket->address );
            ++done;
        }

        void disconnectEvent ( Socket *socket ) override
        {
            EventManager::get().stop();
        }

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            if ( socket->getAsUDP().isConnectionLess() )
                bindedMsg = msg;
            else
                connectedMsg = msg;

            if ( socket->address.addr.empty() )
                socket->send ( new TestMessage ( "Connection-less message" ), address );

            ++done;
            if ( done >= 6 )
            {
                LOG ( "Stopping because everything happened correctly" );
                EventManager::get().stop();
            }
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( step == 0 )
            {
                if ( !socket->address.addr.empty() )
                    socket->send ( new TestMessage ( "Connection-less message" ), socket->address );
                timer->start ( 1000 );
                ++step;
            }
            else if ( step == 1 )
            {
                if ( socket->isServer() )
                    socket->getAsUDP().listen();
                else
                    socket->getAsUDP().connect();

                socket->setPacketLoss ( PACKET_LOSS );
                socket->setCheckSumFail ( CHECK_SUM_FAIL );
                socket->getAsUDP().setKeepAlive ( LONG_TIMEOUT );

                timer->start ( LONG_TIMEOUT );
                ++step;
            }
            else
            {
                LOG ( "Stopping because of timeout" );
                EventManager::get().stop();
            }
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) )
            , timer ( this )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , timer ( this )
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
    {
        EXPECT_TRUE ( server.socket->isServer() );
        EXPECT_TRUE ( server.accepted.get() );
        EXPECT_TRUE ( server.accepted->isConnected() );
    }

    EXPECT_TRUE ( server.bindedMsg.get() );

    if ( server.bindedMsg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, server.bindedMsg->getMsgType() );
        EXPECT_EQ ( "Connection-less message", server.bindedMsg->getAs<TestMessage>().str );
    }

    EXPECT_TRUE ( server.connectedMsg.get() );

    if ( server.connectedMsg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, server.connectedMsg->getMsgType() );
        EXPECT_EQ ( "Connected message", server.connectedMsg->getAs<TestMessage>().str );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_TRUE ( client.bindedMsg.get() );

    if ( client.bindedMsg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, client.bindedMsg->getMsgType() );
        EXPECT_EQ ( "Connection-less message", client.bindedMsg->getAs<TestMessage>().str );
    }

    EXPECT_TRUE ( client.connectedMsg.get() );

    if ( client.connectedMsg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, client.connectedMsg->getMsgType() );
        EXPECT_EQ ( "Accepted message", client.connectedMsg->getAs<TestMessage>().str );
    }

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}

#endif // NOT RELEASE
