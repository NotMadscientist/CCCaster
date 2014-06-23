#include "Test.h"
#include "Log.h"
#include "Event.h"
#include "Socket.h"
#include "DoubleSocket.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <vector>

using namespace std;

TEST ( DoubleSocket, Connect )
{
    struct TestSocket : public DoubleSocket::Owner, public Timer::Owner
    {
        shared_ptr<DoubleSocket> socket, accepted;
        Timer timer;

        void acceptEvent ( DoubleSocket *serverSocket ) { accepted = serverSocket->accept ( this ); }

        void timerExpired ( Timer *timer ) { EventManager::get().stop(); }

        TestSocket ( unsigned port )
            : socket ( DoubleSocket::listen ( this, port ) ), timer ( this )
        {
            timer.start ( 1000 );
        }

        TestSocket ( const string& address, unsigned port )
            : socket ( DoubleSocket::connect ( this, address, port ) ), timer ( this )
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
