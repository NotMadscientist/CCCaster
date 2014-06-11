#include "Socket.h"
#include "EventManager.h"
#include "Log.h"

#include <windows.h>

#include <vector>
#include <cstdlib>
#include <cstdio>

using namespace std;

struct Test : public Socket::Owner
{
    void acceptEvent ( Socket *serverSocket )
    {
        shared_ptr<Socket> socket ( serverSocket->accept ( *this ) );
    }

    void connectEvent ( Socket *socket )
    {
    }

    void disconnectEvent ( Socket *socket )
    {
    }

    void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address )
    {
    }
};

int main ( int argc, char *argv[] )
{
    srand ( time ( 0 ) );
    NL::init();
    Log::open();

    Test test;
    shared_ptr<Socket> socket;

    try
    {
        if ( argc == 2 )
            socket.reset ( Socket::listen ( test, atoi ( argv[1] ), Socket::TCP ) );
        else if ( argc == 3 )
            socket.reset ( Socket::connect ( test, argv[1], atoi ( argv[2] ), Socket::TCP ) );
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    EventManager::get().start();

    Log::close();
    return 0;
}
