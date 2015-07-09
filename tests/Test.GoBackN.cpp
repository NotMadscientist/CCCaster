#ifndef RELEASE

#include "Test.Socket.hpp"
#include "UdpSocket.hpp"
#include "GoBackN.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace std;


#define PACKET_LOSS     50
#define CHECK_SUM_FAIL  50
#define LONG_TIMEOUT    ( 120 * 1000 )


struct TestClass : public GoBackN::Owner, public Socket::Owner, public Timer::Owner
{
    virtual void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override {}
    virtual void goBackNRecvRaw ( GoBackN *gbn, const MsgPtr& msg ) override {}
    virtual void goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg ) override {}
    virtual void goBackNTimeout ( GoBackN *gbn ) override {}

    virtual void socketAccepted ( Socket *socket ) override {}
    virtual void socketConnected ( Socket *socket ) override {}
    virtual void socketDisconnected ( Socket *socket ) override {}

    virtual void timerExpired ( Timer *timer ) override {}
};


TEST ( GoBackN, SendOnce )
{
    struct TestSocket : public TestClass
    {
        SocketPtr socket;
        IpAddrPort address;
        GoBackN gbn;
        Timer timer;
        MsgPtr msg;

        void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            socket->send ( msg, address );
        }

        void goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            this->msg = msg;

            LOG ( "Stopping because the msg was received" );
            EventManager::get().stop();
        }

        void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            if ( this->address.empty() )
                this->address = address;

            gbn.recvFromSocket ( msg );
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( socket->isClient() )
            {
                gbn.sendViaGoBackN ( new TestMessage ( "Hello server!" ) );
            }
            else
            {
                LOG ( "Stopping because of timeout" );
                EventManager::get().stop();
            }
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) )
            , gbn ( this ), timer ( this )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( LONG_TIMEOUT );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , address ( address, port ), gbn ( this ), timer ( this )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( 1000 );
        }
    };

    TimerManager::get().initialize();
    SocketManager::get().initialize();

    TestSocket server ( 0 );
    TestSocket client ( "127.0.0.1", server.socket->address.port );

    EventManager::get().start();

    EXPECT_TRUE ( server.msg.get() );

    if ( server.msg.get() )
    {
        EXPECT_EQ ( MsgType::TestMessage, server.msg->getMsgType() );
        EXPECT_EQ ( "Hello server!", server.msg->getAs<TestMessage>().str );
    }

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}

TEST ( GoBackN, SendSequential )
{
    struct TestSocket : public TestClass
    {
        SocketPtr socket;
        IpAddrPort address;
        GoBackN gbn;
        Timer timer;
        vector<MsgPtr> msgs;

        void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            socket->send ( msg, address );
        }

        void goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            msgs.push_back ( msg );

            if ( msgs.size() == 5 )
            {
                LOG ( "Stopping because all msgs have been received" );
                EventManager::get().stop();
            }
        }

        void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            if ( this->address.empty() )
                this->address = address;

            gbn.recvFromSocket ( msg );
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( socket->isClient() )
            {
                gbn.sendViaGoBackN ( new TestMessage ( "Message 1" ) );
                gbn.sendViaGoBackN ( new TestMessage ( "Message 2" ) );
                gbn.sendViaGoBackN ( new TestMessage ( "Message 3" ) );
                gbn.sendViaGoBackN ( new TestMessage ( "Message 4" ) );
                gbn.sendViaGoBackN ( new TestMessage ( "Message 5" ) );
            }
            else
            {
                LOG ( "Stopping because of timeout" );
                EventManager::get().stop();
            }
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) )
            , gbn ( this ), timer ( this )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( LONG_TIMEOUT );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , address ( address, port ), gbn ( this ), timer ( this )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( 1000 );
        }
    };

    TimerManager::get().initialize();
    SocketManager::get().initialize();

    TestSocket server ( 0 );
    TestSocket client ( "127.0.0.1", server.socket->address.port );

    EventManager::get().start();

    EXPECT_EQ ( 5, server.msgs.size() );

    for ( size_t i = 0; i < server.msgs.size(); ++i )
    {
        LOG ( "Server got '%s'", server.msgs[i]->getAs<TestMessage>().str );
        EXPECT_EQ ( MsgType::TestMessage, server.msgs[i]->getMsgType() );
        EXPECT_EQ ( format ( "Message %u", i + 1 ), server.msgs[i]->getAs<TestMessage>().str );
    }

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}

TEST ( GoBackN, SendAndRecv )
{
    static int done = 0;
    done = 0;

    struct TestSocket : public TestClass
    {
        SocketPtr socket;
        IpAddrPort address;
        GoBackN gbn;
        Timer timer;
        vector<MsgPtr> msgs;
        bool sent;

        void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            if ( ! address.empty() )
                socket->send ( msg, address );
        }

        void goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            msgs.push_back ( msg );

            if ( msgs.size() == 5 )
                ++done;

            if ( done >= 2 )
            {
                LOG ( "Stopping because all msgs have been received" );
                EventManager::get().stop();
            }
        }

        void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            if ( this->address.empty() )
                this->address = address;

            gbn.recvFromSocket ( msg );
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( ! sent )
            {
                gbn.sendViaGoBackN ( new TestMessage ( socket->isClient() ? "Client 1" : "Server 1" ) );
                gbn.sendViaGoBackN ( new TestMessage ( socket->isClient() ? "Client 2" : "Server 2" ) );
                gbn.sendViaGoBackN ( new TestMessage ( socket->isClient() ? "Client 3" : "Server 3" ) );
                gbn.sendViaGoBackN ( new TestMessage ( socket->isClient() ? "Client 4" : "Server 4" ) );
                gbn.sendViaGoBackN ( new TestMessage ( socket->isClient() ? "Client 5" : "Server 5" ) );
                sent = true;
                timer->start ( LONG_TIMEOUT );
            }
            else
            {
                LOG ( "Stopping because of timeout" );
                EventManager::get().stop();
            }
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) )
            , gbn ( this ), timer ( this ), sent ( false )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) )
            , address ( address, port ), gbn ( this ), timer ( this ), sent ( false )
        {
            socket->setPacketLoss ( PACKET_LOSS );
            socket->setCheckSumFail ( CHECK_SUM_FAIL );
            timer.start ( 1000 );
        }
    };

    TimerManager::get().initialize();
    SocketManager::get().initialize();

    TestSocket server ( 0 );
    TestSocket client ( "127.0.0.1", server.socket->address.port );

    EventManager::get().start();

    EXPECT_EQ ( 5, server.msgs.size() );

    for ( size_t i = 0; i < server.msgs.size(); ++i )
    {
        LOG ( "Server got '%s'", server.msgs[i]->getAs<TestMessage>().str );
        EXPECT_EQ ( MsgType::TestMessage, server.msgs[i]->getMsgType() );
        EXPECT_EQ ( format ( "Client %u", i + 1 ), server.msgs[i]->getAs<TestMessage>().str );
    }

    EXPECT_EQ ( 5, client.msgs.size() );

    for ( size_t i = 0; i < client.msgs.size(); ++i )
    {
        LOG ( "Client got '%s'", client.msgs[i]->getAs<TestMessage>().str );
        EXPECT_EQ ( MsgType::TestMessage, client.msgs[i]->getMsgType() );
        EXPECT_EQ ( format ( "Server %u", i + 1 ), client.msgs[i]->getAs<TestMessage>().str );
    }

    EXPECT_EQ ( 2, done );

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}

TEST ( GoBackN, Timeout )
{
    static int done = 0;
    done = 0;

    struct TestSocket : public TestClass
    {
        SocketPtr socket;
        IpAddrPort address;
        GoBackN gbn;
        Timer timer;
        bool properTimeout;
        int stage;

        void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override
        {
            socket->send ( msg, address );
        }

        void goBackNTimeout ( GoBackN *gbn ) override
        {
            properTimeout = true;
            ++done;

            if ( done >= 2 )
            {
                LOG ( "Stopping because both GoBackN instances have timed out" );
                EventManager::get().stop();
            }
        }

        void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
        {
            if ( this->address.empty() )
                this->address = address;

            gbn.recvFromSocket ( msg );
        }

        void timerExpired ( Timer *timer ) override
        {
            if ( stage == 0 )
            {
                if ( socket->isClient() )
                    gbn.sendViaGoBackN ( new TestMessage ( "Hello server!" ) );
                timer->start ( 1000 );
            }
            else if ( stage == 1 )
            {
                socket->setPacketLoss ( 100 );
                timer->start ( 60 * 1000 );
            }
            else
            {
                LOG ( "Stopping because of timeout" );
                EventManager::get().stop();
            }

            ++stage;
        }

        TestSocket ( uint16_t port )
            : socket ( UdpSocket::bind ( this, port ) ), gbn ( this, DEFAULT_SEND_INTERVAL, 1000 )
            , timer ( this ), properTimeout ( false ), stage ( 0 )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, uint16_t port )
            : socket ( UdpSocket::bind ( this, IpAddrPort ( address, port ) ) ), address ( address, port )
            , gbn ( this, DEFAULT_SEND_INTERVAL, 1000 ), timer ( this ), properTimeout ( false ), stage ( 0 )
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
    EXPECT_TRUE ( client.socket.get() );
    EXPECT_TRUE ( server.properTimeout );
    EXPECT_TRUE ( client.properTimeout );
    EXPECT_EQ ( 2, done );

    SocketManager::get().deinitialize();
    TimerManager::get().deinitialize();
}

#endif // NOT RELEASE
