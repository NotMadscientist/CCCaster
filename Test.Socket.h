#pragma once

#include "Test.h"
#include "Log.h"
#include "Event.h"
#include "Socket.h"
#include "ReliableUdp.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <vector>

using namespace std;

template<typename T, int timeout>
struct BaseTestSocket : public Socket::Owner, public Timer::Owner
{
    shared_ptr<Socket> socket, accepted;
    Timer timer;

    BaseTestSocket ( unsigned port )
        : socket ( T::listen ( this, port, Protocol::TCP ) ), timer ( this )
    {
        timer.start ( timeout );
    }

    BaseTestSocket ( const string& address, unsigned port )
        : socket ( T::connect ( this, address, port, Protocol::TCP ) ), timer ( this )
    {
        timer.start ( timeout );
    }
};

template<int timeout>
struct BaseTestSocket<ReliableUdp, timeout> : public Socket::Owner, public Timer::Owner
{
    shared_ptr<Socket> socket, accepted;
    Timer timer;

    BaseTestSocket ( unsigned port )
        : socket ( ReliableUdp::listen ( this, port ) ), timer ( this )
    {
        timer.start ( timeout );
    }

    BaseTestSocket ( const string& address, unsigned port )
        : socket ( ReliableUdp::connect ( this, address, port ) ), timer ( this )
    {
        timer.start ( timeout );
    }
};

#define TEST_CONNECT(T, PREFIX, LOSS, TIMEOUT)                                                                      \
    TEST ( T, PREFIX ## Connect ) {                                                                                 \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, TIMEOUT> {                                                     \
            void acceptEvent ( Socket *serverSocket ) override {                                                    \
                accepted = serverSocket->accept ( this );                                                           \
                ++done;                                                                                             \
                if ( done >= 2 ) {                                                                                  \
                    LOG ( "Stopping because connected" );                                                           \
                    EventManager::get().stop();                                                                     \
                }                                                                                                   \
            }                                                                                                       \
            void connectEvent ( Socket *socket ) override {                                                         \
                ++done;                                                                                             \
                if ( done >= 2 ) {                                                                                  \
                    LOG ( "Stopping because connected" );                                                           \
                    EventManager::get().stop();                                                                     \
                }                                                                                                   \
            }                                                                                                       \
            void timerExpired ( Timer *timer ) override {                                                           \
                LOG ( "Stopping because of timeout" );                                                              \
                EventManager::get().stop();                                                                         \
            }                                                                                                       \
            TestSocket ( unsigned port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, unsigned port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TestSocket server ( TEST_LOCAL_PORT );                                                                      \
        TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );                                                         \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( server.socket.get() );                                                                        \
        if ( server.socket.get() )                                                                                  \
            EXPECT_TRUE ( server.socket->isServer() );                                                              \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_EQ ( server.socket->getPendingCount(), 0 );                                                          \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
    }

#define TEST_TIMEOUT(T, PREFIX, LOSS, TIMEOUT)                                                                      \
    TEST ( T, PREFIX ## Timeout ) {                                                                                 \
        struct TestSocket : public BaseTestSocket<T, TIMEOUT> {                                                     \
            void timerExpired ( Timer *timer ) override { EventManager::get().stop(); }                             \
            TestSocket ( const string& address, unsigned port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );                                                         \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_FALSE ( client.socket->isConnected() );                                                          \
    }

#define TEST_SEND(T, PREFIX, LOSS, TIMEOUT)                                                                         \
    TEST ( T, PREFIX ## Send ) {                                                                                    \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, TIMEOUT> {                                                     \
            MsgPtr msg;                                                                                             \
            void acceptEvent ( Socket *serverSocket ) override {                                                    \
                accepted = serverSocket->accept ( this );                                                           \
                accepted->send ( new TestMessage ( "Hello client!" ) );                                             \
            }                                                                                                       \
            void connectEvent ( Socket *socket ) override {                                                         \
                socket->send ( new TestMessage ( "Hello server!" ) );                                               \
            }                                                                                                       \
            void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {              \
                this->msg = msg;                                                                                    \
                ++done;                                                                                             \
                if ( done >= 2 ) {                                                                                  \
                    LOG ( "Stopping because connected" );                                                           \
                    EventManager::get().stop();                                                                     \
                }                                                                                                   \
            }                                                                                                       \
            void timerExpired ( Timer *timer ) override {                                                           \
                LOG ( "Stopping because of timeout" );                                                              \
                EventManager::get().stop();                                                                         \
            }                                                                                                       \
            TestSocket ( unsigned port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, unsigned port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TestSocket server ( TEST_LOCAL_PORT );                                                                      \
        TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );                                                         \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( server.socket.get() );                                                                        \
        if ( server.socket.get() )                                                                                  \
            EXPECT_TRUE ( server.socket->isServer() );                                                              \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_EQ ( server.socket->getPendingCount(), 0 );                                                          \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( server.msg->getType(), MsgType::TestMessage );                                              \
            EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_TRUE ( client.msg.get() );                                                                           \
        if ( client.msg.get() ) {                                                                                   \
            EXPECT_EQ ( client.msg->getType(), MsgType::TestMessage );                                              \
            EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );                                    \
        }                                                                                                           \
    }

#define TEST_SEND_PARTIAL(T, PREFIX)                                                                                \
    TEST ( T, PREFIX ## SendPartial ) {                                                                             \
        struct TestSocket : public BaseTestSocket<T, 1000> {                                                        \
            MsgPtr msg;                                                                                             \
            string buffer;                                                                                          \
            void acceptEvent ( Socket *serverSocket ) override { accepted = serverSocket->accept ( this ); }        \
            void connectEvent ( Socket *socket ) override {                                                         \
                socket->send ( &buffer[0], 5 );                                                                     \
                buffer.erase ( 0, 5 );                                                                              \
            }                                                                                                       \
            void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {              \
                this->msg = msg;                                                                                    \
            }                                                                                                       \
            void timerExpired ( Timer *timer ) override {                                                           \
                if ( socket->isClient() )                                                                           \
                    socket->send ( &buffer[0], buffer.size() );                                                     \
                else                                                                                                \
                    EventManager::get().stop();                                                                     \
            }                                                                                                       \
            TestSocket ( unsigned port ) : BaseTestSocket ( port ) { timer.start ( 2000 ); }                        \
            TestSocket ( const string& address, unsigned port ) : BaseTestSocket ( address, port ),                 \
                buffer ( Serializable::encode ( new TestMessage ( "Hello server!" ) ) ) {}                          \
        };                                                                                                          \
        TestSocket server ( TEST_LOCAL_PORT );                                                                      \
        TestSocket client ( "127.0.0.1", TEST_LOCAL_PORT );                                                         \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( server.socket.get() );                                                                        \
        if ( server.socket.get() )                                                                                  \
            EXPECT_TRUE ( server.socket->isServer() );                                                              \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_EQ ( server.socket->getPendingCount(), 0 );                                                          \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( server.msg->getType(), MsgType::TestMessage );                                              \
            EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_FALSE ( client.msg.get() );                                                                          \
    }
