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

    Test() : timer ( *this ) {}

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
        if ( socket.get() && !socket->isConnected() )
        {
            socket.reset();
            timer->start ( 30000 );
            return;
        }

        EventManager::get().stop();
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
        }
        else if ( argc == 3 )
        {
            test.socket.reset ( Socket::connect ( test, argv[1], atoi ( argv[2] ), Socket::TCP ) );
            test.timer.start ( 5000 );
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
