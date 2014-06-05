#include "Socket.h"
#include "Thread.h"
#include "Log.h"

#include <windows.h>

#include <vector>
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
        acceptId = id;
        acceptCond.signal();
    }

    void disconnected ( uint32_t id )
    {
        LOG ( "disconnected" );
    }

public:

    Server() : acceptId ( 0 ) {}

    uint32_t accept()
    {
        uint32_t id = 0;
        LOCK ( mutex );
        if ( !acceptId )
            acceptCond.wait ( mutex );
        id = acceptId;
        acceptId = 0;
        return id;
    }
};

class Client : public Socket
{
    CondVar connectCond;

protected:

    void connected ( uint32_t id )
    {
        connectCond.signal();
    }

    void disconnected ( uint32_t id )
    {
        LOG ( "disconnected" );
    }

public:

    void wait ( long timeout = 0 )
    {
        LOCK ( mutex );
        while ( !isConnected() )
        {
            if ( timeout )
                connectCond.wait ( mutex, timeout );
            else
                connectCond.wait ( mutex );
        }
    }
};

int main ( int argc, char *argv[] )
{
    NL::init();
    Log::open();

    try
    {
        if ( argc == 2 )
        {
            shared_ptr<Server> server ( new Server() );
            server->listen ( atoi ( argv[1] ) );

            // for ( ;; )
            {
                uint32_t id = server->accept();

                LOG ( "%s [%08x]", server->remoteAddr ( id ).c_str(), id );

                Sleep ( 3000 );

                // server.disconnect ( id );

                // Sleep ( 1000 );
            }
        }
        else if ( argc == 3 )
        {
            vector<shared_ptr<Client>> clients;

            // for ( ;; )
            {
                shared_ptr<Client> client ( new Client() );
                clients.push_back ( client );
                client->connect ( argv[1], atoi ( argv[2] ) );
                client->wait();

                if ( client->isConnected() )
                    LOG ( "%s", client->remoteAddr().c_str() );
                else
                    LOG ( "connect failed" );

                Sleep ( 1000 );

                client->disconnect();

                Sleep ( 1000 );
            }
        }
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    Log::close();
    return 0;
}
