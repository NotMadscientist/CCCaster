#include "Socket.h"
#include "Thread.h"

#include <cstdlib>
#include <cstdio>

using namespace std;

class Server : public Socket
{
    CondVar acceptCond;
    uint32_t acceptId;

protected:
    void accepted ( uint32_t id )
    {
        LOCK ( mutex );
        acceptId = id;
        acceptCond.signal();
    }

public:
    Server() : acceptId ( 0 ) {}

    uint32_t accept()
    {
        LOCK ( mutex );
        if ( !acceptId )
            acceptCond.wait ( mutex );
        return acceptId;
    }
};

class Client : public Socket
{
    CondVar connectCond;

protected:
    void connected()
    {
        LOCK ( mutex );
        connectCond.signal();
    }

public:
    void wait()
    {
        LOCK ( mutex );
        if ( !isConnected() )
            connectCond.wait ( mutex );
    }
};

int main ( int argc, char *argv[] )
{
    if ( argc == 2 )
    {
        Server server;
        server.listen ( atoi ( argv[1] ) );
        uint32_t id = server.accept();

        printf ( "%s\n", server.remoteAddr ( id ).c_str() );
    }
    else if ( argc == 3 )
    {
        Client client;
        client.connect ( argv[1], atoi ( argv[2] ) );
        client.wait();

        printf ( "%s\n", client.remoteAddr().c_str() );
    }

    return 0;
}
