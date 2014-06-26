#include "Test.h"
#include "Log.h"
#include "Event.h"
#include "Socket.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <vector>

using namespace std;

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
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
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

    EXPECT_TRUE ( server.accepted.get() );
    if ( server.accepted.get() )
        EXPECT_TRUE ( server.accepted->isConnected() );

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
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
            timer.start ( 1000 );
        }
    };

    TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );

    EventManager::get().start();

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
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
            accepted->send ( new TestMessage ( "Hello client!" ) );
        }

        void connectEvent ( Socket *socket )
        {
            socket->send ( new TestMessage ( "Hello server!" ) );
        }

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
        {
            this->msg = msg;
        }

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
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

    EXPECT_TRUE ( server.accepted.get() );
    if ( server.accepted.get() )
        EXPECT_TRUE ( server.accepted->isConnected() );

    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->getType(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->getType(), MsgType::TestMessage );
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

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
        {
            this->msg = msg;

            if ( socket->getRemoteAddress().addr.empty() )
            {
                socket->send ( new TestMessage ( "Hello client!" ), address );
                sent = true;
            }
        }

        void timerExpired ( Timer *timer )
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
            : socket ( Socket::listen ( this, port, Protocol::UDP ) ), timer ( this ), sent ( false )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::UDP ) ), timer ( this ), sent ( false )
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
        EXPECT_EQ ( server.msg->getType(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_TRUE ( client.msg.get() );

    if ( client.msg.get() )
    {
        EXPECT_EQ ( client.msg->getType(), MsgType::TestMessage );
        EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );
    }
}

TEST ( Socket, TcpSendPartial )
{
    struct TestSocket : public Socket::Owner, public Timer::Owner
    {
        shared_ptr<Socket> socket, accepted;
        Timer timer;
        MsgPtr msg;
        string buffer;

        void acceptEvent ( Socket *serverSocket )
        {
            accepted = serverSocket->accept ( this );
        }

        void connectEvent ( Socket *socket )
        {
            socket->send ( &buffer[0], 5 );
            buffer.erase ( 0, 5 );
        }

        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
        {
            this->msg = msg;
        }

        void timerExpired ( Timer *timer )
        {
            if ( socket->isClient() )
            {
                socket->send ( &buffer[0], buffer.size() );
            }
            else
            {
                EventManager::get().stop();
            }
        }

        TestSocket ( unsigned port )
            : socket ( Socket::listen ( this, port, Protocol::TCP ) ), timer ( this )
        {
            timer.start ( 2000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
            , buffer ( Serializable::encode ( new TestMessage ( "Hello server!" ) ) )
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

    EXPECT_TRUE ( server.accepted.get() );
    if ( server.accepted.get() )
        EXPECT_TRUE ( server.accepted->isConnected() );

    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( server.msg->getType(), MsgType::TestMessage );
        EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );
    }

    EXPECT_TRUE ( client.socket.get() );
    if ( client.socket.get() )
        EXPECT_TRUE ( client.socket->isConnected() );

    EXPECT_FALSE ( client.msg.get() );
}
