#include "Event.h"
#include "Test.h"
#include "Socket.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <memory>

#define TEST_PORT       258258
#define TEST_TIMEOUT    2000

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

// TEST ( SocketTest, SendTcpMessage )
// {
    // struct TestSocket : public Socket::Owner, public Timer::Owner
    // {
        // shared_ptr<Socket> socket, accept;
        // bool connected;

        // void acceptEvent ( Socket *serverSocket ) { accept = serverSocket->accept ( this ); connected = accept.get(); }
        // void connectEvent ( Socket *socket ) { connected = socket->isConnected(); }
        // void disconnectEvent ( Socket *socket ) { connected = false; }

        // void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        // TestSocket ( unsigned port )
            // : socket ( Socket::listen ( this, port, Protocol::TCP ) ), connected ( false ) {}
        // TestSocket ( const string& address, unsigned port )
            // : socket ( Socket::connect ( this, address, port, Protocol::TCP ) ), connected ( false ) {}
    // };

    // TestSocket server ( TEST_PORT );
    // TestSocket client ( "127.0.0.1", TEST_PORT );

    // Timer timer ( &client );
    // timer.start ( TEST_TIMEOUT );

    // EventManager::get().start();

    // EXPECT_TRUE ( server.connected );
    // EXPECT_TRUE ( client.connected );
// }
