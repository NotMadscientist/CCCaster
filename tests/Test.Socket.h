#pragma once

#include "Logger.h"
#include "EventManager.h"
#include "SocketManager.h"
#include "Socket.h"
#include "TimerManager.h"
#include "Timer.h"
#include "Protocol.h"

#include <gtest/gtest.h>
#include <cereal/types/string.hpp>

#include <vector>
#include <string>

using namespace std;


struct TestMessage : public SerializableSequence
{
    std::string str;

    TestMessage() {}

    TestMessage ( const std::string& str ) : str ( str ) {}

    PROTOCOL_BOILERPLATE ( str )
};


template<typename T, uint64_t keepAlive, uint64_t timeout>
struct BaseTestSocket : public Socket::Owner, public Timer::Owner
{
    SocketPtr socket, accepted;
    Timer timer;

    BaseTestSocket ( uint16_t port )
        : socket ( T::listen ( this, port ) ), timer ( this )
    {
        timer.start ( timeout );
    }

    BaseTestSocket ( const string& address, uint16_t port )
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
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
        EXPECT_EQ ( 2, done );                                                                                      \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
    }

#define TEST_TIMEOUT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                                  \
    TEST ( T, Timeout ) {                                                                                           \
        struct TestSocket : public BaseTestSocket<T, KEEP_ALIVE, TIMEOUT> {                                         \
            void timerExpired ( Timer *timer ) override { EventManager::get().stop(); }                             \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket client ( "127.0.0.1", 39393 );                                                                   \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_FALSE ( client.socket->isConnected() );                                                          \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
    }

#define TEST_DISCONNECT_CLIENT(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                        \
    TEST ( T, DisconnectClient ) {                                                                                  \
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
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
        EXPECT_EQ ( 2, done );                                                                                      \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
    }

#define TEST_DISCONNECT_ACCEPTED(T, LOSS, KEEP_ALIVE, TIMEOUT)                                                      \
    TEST ( T, DisconnectAccepted ) {                                                                                \
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
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
        EXPECT_EQ ( 2, done );                                                                                      \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( server.socket.get() );                                                                        \
        if ( server.socket.get() )                                                                                  \
            EXPECT_TRUE ( server.socket->isServer() );                                                              \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( MsgType::TestMessage, server.msg->getMsgType() );                                           \
            EXPECT_EQ ( "Hello server!", server.msg->getAs<TestMessage>().str );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_TRUE ( client.msg.get() );                                                                           \
        if ( client.msg.get() ) {                                                                                   \
            EXPECT_EQ ( MsgType::TestMessage, client.msg->getMsgType() );                                           \
            EXPECT_EQ ( "Hello client!", client.msg->getAs<TestMessage>().str );                                    \
        }                                                                                                           \
        EXPECT_EQ ( 2, done );                                                                                      \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { socket->setPacketLoss ( LOSS ); }              \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port )                  \
            { socket->setPacketLoss ( LOSS ); }                                                                     \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
        EventManager::get().start();                                                                                \
        EXPECT_FALSE ( server.socket.get() );                                                                       \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( MsgType::TestMessage, server.msg->getMsgType() );                                           \
            EXPECT_EQ ( "Hello server!", server.msg->getAs<TestMessage>().str );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_TRUE ( client.msg.get() );                                                                           \
        if ( client.msg.get() ) {                                                                                   \
            EXPECT_EQ ( MsgType::TestMessage, client.msg->getMsgType() );                                           \
            EXPECT_EQ ( "Hello client!", client.msg->getAs<TestMessage>().str );                                    \
        }                                                                                                           \
        EXPECT_EQ ( 2, done );                                                                                      \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
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
            TestSocket ( uint16_t port ) : BaseTestSocket ( port ) { timer.start ( 2000 ); }                        \
            TestSocket ( const string& address, uint16_t port ) : BaseTestSocket ( address, port ) {                \
                buffer = Serializable::encode ( new TestMessage ( "Hello server!" ) );                              \
                LOG ( "buffer=[ %s ]", toBase64 ( buffer ) );                                                       \
            }                                                                                                       \
        };                                                                                                          \
        TimerManager::get().initialize();                                                                           \
        SocketManager::get().initialize();                                                                          \
        TestSocket server ( 0 );                                                                                    \
        TestSocket client ( "127.0.0.1", server.socket->address.port );                                             \
        EventManager::get().start();                                                                                \
        EXPECT_TRUE ( server.socket.get() );                                                                        \
        if ( server.socket.get() )                                                                                  \
            EXPECT_TRUE ( server.socket->isServer() );                                                              \
        EXPECT_TRUE ( server.accepted.get() );                                                                      \
        if ( server.accepted.get() )                                                                                \
            EXPECT_TRUE ( server.accepted->isConnected() );                                                         \
        EXPECT_TRUE ( server.msg.get() );                                                                           \
        if ( server.msg.get() ) {                                                                                   \
            EXPECT_EQ ( MsgType::TestMessage, server.msg->getMsgType() );                                           \
            EXPECT_EQ ( "Hello server!", server.msg->getAs<TestMessage>().str );                                    \
        }                                                                                                           \
        EXPECT_TRUE ( client.socket.get() );                                                                        \
        if ( client.socket.get() )                                                                                  \
            EXPECT_TRUE ( client.socket->isConnected() );                                                           \
        EXPECT_FALSE ( client.msg.get() );                                                                          \
        SocketManager::get().deinitialize();                                                                        \
        TimerManager::get().deinitialize();                                                                         \
    }
