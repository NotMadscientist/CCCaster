#include "Test.h"
#include "Log.h"
#include "Event.h"
#include "Socket.h"
#include "Timer.h"

#include <gtest/gtest.h>

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
                timer.start ( 1000 );
            }
        };

        TestSocket client ( "google.com" , 23456 );

        EventManager::get().start();

        LOG ( "client.socket->isConnected()=%s; expected false", client.socket->isConnected() ? "true" : "false" );
    }

    LOG ( "Finished all tests" );

    return result;
}
