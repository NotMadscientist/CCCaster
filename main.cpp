#include "Event.h"
#include "Socket.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>

#include <vector>
#include <cstdlib>
#include <cstdio>

using namespace std;

struct Test : public Socket::Owner, public Timer::Owner
{
    shared_ptr<Socket> socket, accepted;
    Timer timer;
    uint32_t tick;

    Test() : timer ( *this ), tick ( 0 ) {}

    void acceptEvent ( Socket *serverSocket )
    {
        accepted.reset ( serverSocket->accept ( *this ) );
        accepted->send ( accepted->getRemoteAddress() );
    }

    void connectEvent ( Socket *socket )
    {
        // this->socket.reset();
    }

    void disconnectEvent ( Socket *socket )
    {
        // this->socket.reset();
    }

    void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
    {
        MsgPtr msg = Serializable::decode ( bytes, len );

        if ( !msg.get() )
        {
            LOG ( "Failed to decode [ %u bytes ]", len );
        }
        else
        {
            switch ( msg->type().value )
            {
                case MsgType::IpAddrPort:
                {
                    LOG ( "IpAddrPort '%s'", static_cast<IpAddrPort *> ( msg.get() )->c_str() );
                    break;
                }
            }
        }

        // this->socket.reset();

        // EventManager::get().stop();
    }

    void timerExpired ( Timer *timer )
    {
        LOG ( "tick %u", tick );

        if ( tick == 0 )
        {
            socket->disconnect();
            socket.reset();

            timer->start ( 1000.0 );
        }
        else if ( tick == 1 )
        {
            socket.reset ( Socket::listen ( *this, 1235, Socket::TCP ) );

            timer->start ( 10000.0 );
        }
        else
        {
            EventManager::get().stop();
        }

        ++tick;
    }
};

int main ( int argc, char *argv[] )
{
    srand ( time ( 0 ) );
    NL::init();
    Log::open();

    Test test;

    try
    {
        if ( argc == 2 )
        {
            test.socket.reset ( Socket::listen ( test, atoi ( argv[1] ), Socket::TCP ) );
            // test.timer.start ( 1000.0 );
        }
        else if ( argc == 3 )
        {
            test.socket.reset ( Socket::connect ( test, argv[1], atoi ( argv[2] ), Socket::TCP ) );
        }
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    EventManager::get().start();

    Log::close();
    return 0;
}
