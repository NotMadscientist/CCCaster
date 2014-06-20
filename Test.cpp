#include "Event.h"
#include "Test.h"
#include "Socket.h"
#include "Timer.h"
#include "GoBackN.h"

#include <gtest/gtest.h>

#include <memory>

#define TEST_PORT       258258
#define TEST_TIMEOUT    1000

using namespace std;

int RunAllTests ( int& argc, char *argv[] )
{
    testing::InitGoogleTest ( &argc, argv );
    int result = RUN_ALL_TESTS();

    // Final timeout test with EventManager::release();
    {
        struct TestSocket : public Socket::Owner, public Timer::Owner
        {
            shared_ptr<Socket> socket;
            Timer timer;

            void timerExpired ( Timer *timer ) { EventManager::get().release(); }

            TestSocket ( const string& address, unsigned port )
                : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
            {
                timer.start ( TEST_TIMEOUT );
            }
        };

        TestSocket client ( "google.com" , 23456 );

        EventManager::get().start();

        assert ( client.socket->isConnected() == false );
    }

    return result;
}

TEST ( Socket, TcpConnect )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket, accepted;
        Timer timer;

        void acceptEvent ( Socket *serverSocket ) { accepted = serverSocket->accept ( this ); }

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( TEST_TIMEOUT );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( TEST_TIMEOUT );
        }
    };

    TestSocket server ( TEST_PORT );
    TestSocket client ( "127.0.0.1", TEST_PORT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket->isConnected() );
    EXPECT_TRUE ( server.accepted->isConnected() );
    EXPECT_TRUE ( client.socket->isConnected() );
}

TEST ( Socket, TcpTimeout )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket;
        Timer timer;

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( TEST_TIMEOUT );
        }
    };

    TestSocket client ( "127.0.0.1", TEST_PORT );

    EventManager::get().start();

    EXPECT_FALSE ( client.socket->isConnected() );
}

TEST ( Socket, TcpSend )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket, accepted;
        Timer timer;
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
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( TEST_TIMEOUT );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( TEST_TIMEOUT );
        }
    };

    TestSocket server ( TEST_PORT );
    TestSocket client ( "127.0.0.1", TEST_PORT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket->isConnected() );
    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket->isConnected() );
    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );
    }
}

TEST ( Socket, UdpSend )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket;
        Timer timer;
        MsgPtr msg;
        bool sent;

        void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
        {
            msg = Serializable::decode ( bytes, len );

            if ( socket->getRemoteAddress().addr.empty() )
            {
                socket->send ( TestMessage ( "Hello client!" ), address );
                sent = true;
            }
        }

        void timerExpired ( Timer *timer )
        {
            if ( !sent )
            {
                if ( !socket->getRemoteAddress().addr.empty() )
                {
                    socket->send ( TestMessage ( "Hello server!" ) );
                    sent = true;
                }

                timer->start ( TEST_TIMEOUT );
            }
            else
            {
                EventManager::get().stop();
            }
        }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::UDP ) ), timer ( this ), sent ( false )
        {
            timer.start ( TEST_TIMEOUT );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::UDP ) ), timer ( this ), sent ( false )
        {
            timer.start ( TEST_TIMEOUT );
        }
    };

    TestSocket server ( TEST_PORT );
    TestSocket client ( "127.0.0.1", TEST_PORT );

    EventManager::get().start();

    EXPECT_TRUE ( server.socket->isConnected() );
    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket->isConnected() );
    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->type(), MsgType::TestMessage );
        EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );
    }
}
