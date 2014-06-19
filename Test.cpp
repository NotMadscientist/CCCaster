#include "Event.h"
#include "Test.h"
#include "Socket.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <memory>

#define TEST_PORT       258258
#define TEST_TIMEOUT    1000

using namespace std;

TEST ( SocketTest, ConnectTcp )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket, accepted;

        void acceptEvent ( Socket *serverSocket ) { accepted = serverSocket->accept ( this ); }

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ) {}
        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ) {}
    };

    TestSocket server ( TEST_PORT );
    TestSocket client ( "127.0.0.1", TEST_PORT );

    Timer timer ( &client );
    timer.start ( TEST_TIMEOUT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket->isConnected() );
    EXPECT_TRUE ( server.accepted->isConnected() );
    EXPECT_TRUE ( client.socket->isConnected() );
}

TEST ( SocketTest, ConnectTcpTimeout )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket;

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ) {}
    };

    TestSocket client ( "127.0.0.1", TEST_PORT );

    Timer timer ( &client );
    timer.start ( TEST_TIMEOUT );

    EventManager::get().start();

    EXPECT_FALSE ( client.socket->isConnected() );
}

TEST ( SocketTest, SendTcpMessage )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket, accepted;
        MsgPtr msg;

        void acceptEvent ( Socket *serverSocket )
        {
            accepted = serverSocket->accept ( this );
            accepted->send ( TestMessage ( "Hello client!" ) );
        }

        void connectEvent ( Socket *socket )
        {
            socket->send ( TestMessage ( "Hello server!" ) );
        }

        void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
        {
            msg = Serializable::decode ( bytes, len );
        }

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ) {}
        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ) {}
    };

    TestSocket server ( TEST_PORT );
    TestSocket client ( "127.0.0.1", TEST_PORT );

    Timer timer ( &client );
    timer.start ( TEST_TIMEOUT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket->isConnected() );
    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>()->str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket->isConnected() );
    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( client.msg->getAs<TestMessage>()->str, "Hello client!" );
    }
}
