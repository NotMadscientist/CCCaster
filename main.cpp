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

    void tcpAccepted ( uint32_t id )
    {
        acceptId = id;
        acceptCond.signal();
    }

    void tcpDisconnected ( uint32_t id )
    {
        LOG ( "Disconnected" );
    }

    void tcpReceived ( char *bytes, size_t len, uint32_t id )
    {
        LOG ( "Received '%s' from %08x", string ( bytes, len ).c_str(), id );
    }

    void udpReceived ( char *bytes, size_t len, const string& addr, unsigned port )
    {
        LOG ( "Received '%s' from %s:%u", string ( bytes, len ).c_str(), addr.c_str(), port );
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

    void tcpConnected ( uint32_t id )
    {
        connectCond.signal();
    }

    void tcpDisconnected ( uint32_t id )
    {
        LOG ( "Disconnected" );
    }

    void tcpReceived ( char *bytes, size_t len, uint32_t id )
    {
        LOG ( "Received '%s' from %08x", string ( bytes, len ).c_str(), id );
    }

    void udpReceived ( char *bytes, size_t len, const string& addr, unsigned port )
    {
        LOG ( "Received '%s' from %s:%u", string ( bytes, len ).c_str(), addr.c_str(), port );
    }

public:

    void wait ( long timeout = 0 )
    {
        LOCK ( mutex );
        if ( !isConnected() )
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
    srand ( time ( 0 ) );
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

                server->tcpSend ( "Hi, I'm the server", 18, id );

                Sleep ( 3000 );
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
                client->wait ( 5000 );

                if ( client->isConnected() )
                    LOG ( "%s", client->remoteAddr().c_str() );
                else
                    LOG ( "Connect failed" );

                client->udpSend ( "Hi, I'm the client", 18 );

                Sleep ( 1000 );
            }
        }
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    Socket::release();
    Log::close();
    return 0;
}
