#include "Event.h"
#include "DoubleSocket.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>

#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cassert>

using namespace std;

struct Test : public DoubleSocket::Owner, public Timer::Owner
{
    shared_ptr<DoubleSocket> socket, accepted;
    Timer timer;

    Test() : timer ( this ) {}

    void acceptEvent ( DoubleSocket *serverSocket )
    {
        accepted = serverSocket->accept ( this );
        accepted->sendPrimary ( accepted->getRemoteAddress() );
        // accepted->sendSecondary ( accepted->getRemoteAddress() );
    }

    void connectEvent ( DoubleSocket *socket )
    {
        // this->socket.reset();
    }

    void disconnectEvent ( DoubleSocket *socket )
    {
        // this->socket.reset();
    }

    void readEvent ( DoubleSocket *socket, const MsgPtr& msg, const IpAddrPort& address )
    {
        LOG ( "Read '%s'", TO_C_STR ( msg ) );

        // switch ( msg->type() )
        // {
        // case MsgType::IpAddrPort:
        // {
        // LOG ( "IpAddrPort '%s'", static_cast<IpAddrPort *> ( msg.get() )->c_str() );
        // break;
        // }

        // default:
        // break;
        // }

        // this->socket.reset();

        // EventManager::get().stop();
    }

    void timerExpired ( Timer *timer )
    {
        // if ( socket.get() && !socket->isConnected() )
        // {
        // socket.reset();
        // timer->start ( 30000 );
        // return;
        // }

        if ( socket.get() )
            socket.reset();
        else if ( accepted.get() )
            accepted.reset();
        else
            EventManager::get().stop();

        timer->start ( 5000 );
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
            test.socket = DoubleSocket::listen ( &test, atoi ( argv[1] ) );
            test.timer.start ( 5000 );
        }
        else if ( argc == 3 )
        {
            test.socket = DoubleSocket::connect ( &test, argv[1], atoi ( argv[2] ) );
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
