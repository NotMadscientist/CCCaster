#pragma once

#include "Log.h"
#include "Event.h"
#include "Socket.h"
#include "Timer.h"
#include "Protocol.h"

#include <gtest/gtest.h>
#include <cereal/types/string.hpp>

#include <vector>
#include <string>

#define TEST_LOCAL_PORT 258258

using namespace std;

struct TestMessage : public SerializableSequence
{
    std::string str;

    TestMessage() {}

    TestMessage ( const std::string& str ) : str ( str ) {}

    MsgType getMsgType() const override;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const override { ar ( str ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) override { ar ( str ); }
};


template<typename T, uint64_t keepAlive, uint64_t timeout>
struct BaseTestSocket : public Socket::Owner, public Timer::Owner
{
    shared_ptr<Socket> socket, accepted;
    Timer timer;

    BaseTestSocket ( unsigned port )
        : socket ( T::listen ( this, port ) ), timer ( this )
    {
        timer.start ( timeout );
    }

    BaseTestSocket ( const string& address, unsigned port )
        : socket ( T::connect ( this, IpAddrPort ( address, port ) ) ), timer ( this )
    {
        timer.start ( timeout );
    }
};

#define TEST_CONNECT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                                  \
    TEST ( T, Connect ) {                                                                                           \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
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
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
    }

#define TEST_TIMEOUT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                                  \
    TEST ( T, Timeout ) {                                                                                           \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
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

#define TEST_CLIENT_DISCONNECT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                        \
    TEST ( T, ClientDisconnect ) {                                                                                  \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
            void acceptEvent ( Socket *serverSocket ) override { accepted = serverSocket->accept ( this ); }        \
            void connectEvent ( Socket *socket ) override { socket->disconnect(); ++done; }                         \
            void disconnectEvent ( Socket *socket ) override {                                                      \
                ++done;                                                                                             \
                if ( done >= 2 ) {                                                                                  \
                    LOG ( "Stopping because both sockets have disconnected" );                                      \
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
            EXPECT_FALSE ( server.accepted->isConnected() );                                                        \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_FALSE ( client.socket->isConnected() );                                                          \
    }

#define TEST_SERVER_DISCONNECT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                        \
    TEST ( T, ServerDisconnect ) {                                                                                  \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
            void acceptEvent ( Socket *serverSocket ) override {                                                    \
                accepted = serverSocket->accept ( this );                                                           \
                accepted->disconnect();                                                                             \
                ++done;                                                                                             \
            }                                                                                                       \
            void disconnectEvent ( Socket *socket ) override {                                                      \
                ++done;                                                                                             \
                if ( done >= 2 ) {                                                                                  \
                    LOG ( "Stopping because both sockets have disconnected" );                                      \
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
            EXPECT_FALSE ( server.accepted->isConnected() );                                                        \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_FALSE ( client.socket->isConnected() );                                                          \
    }

#define TEST_SEND(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                                     \
    TEST ( T, Send ) {                                                                                              \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
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
                    LOG ( "Stopping because all msgs have been received" );                                         \
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
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( server.msg->getMsgType(), MsgType::TestMessage );                                           \
            EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_TRUE ( client.msg.get() );                                                                           \
        if ( client.msg.get() ) {                                                                                   \
            EXPECT_EQ ( client.msg->getMsgType(), MsgType::TestMessage );                                           \
            EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );                                    \
        }                                                                                                           \
    }

#define TEST_SEND_WITHOUT_SERVER(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                      \
    TEST ( T, SendWithoutServer ) {                                                                                 \
        static int done = 0;                                                                                        \
        done = 0;                                                                                                   \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
            MsgPtr msg;                                                                                             \
            void acceptEvent ( Socket *serverSocket ) override {                                                    \
                accepted = serverSocket->accept ( this );                                                           \
                accepted->send ( new TestMessage ( "Hello client!" ) );                                             \
                socket.reset();                                                                                     \
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
        EXPECT_FALSE ( server.socket.get() );                                                                       \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( server.msg->getMsgType(), MsgType::TestMessage );                                           \
            EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_TRUE ( client.msg.get() );                                                                           \
        if ( client.msg.get() ) {                                                                                   \
            EXPECT_EQ ( client.msg->getMsgType(), MsgType::TestMessage );                                           \
            EXPECT_EQ ( client.msg->getAs<TestMessage>().str, "Hello client!" );                                    \
        }                                                                                                           \
    }

#define TEST_SEND_PARTIAL(T)                                                                                        \
    TEST ( T, SendPartial ) {                                                                                       \
        struct TestSocket : public BaseTestSocket<T, 0, 1000> {                                                     \
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
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( server.msg->getMsgType(), MsgType::TestMessage );                                           \
            EXPECT_EQ ( server.msg->getAs<TestMessage>().str, "Hello server!" );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_FALSE ( client.msg.get() );                                                                          \
    }
