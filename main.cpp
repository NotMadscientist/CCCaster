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
    IpAddrPort acceptAddress;

protected:

    void tcpAccepted ( const IpAddrPort& address )
    {
        acceptAddress = address;
        acceptCond.signal();
    }

    void tcpDisconnected ( const IpAddrPort& address )
    {
        LOG ( "Disconnected %s", address.c_str() );
    }

    void tcpReceived ( char *bytes, size_t len, const IpAddrPort& address )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), address.c_str() );
    }

    void udpReceived ( char *bytes, size_t len, const IpAddrPort& address )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), address.c_str() );
    }

public:

    IpAddrPort accept()
    {
        IpAddrPort address;
        LOCK ( mutex );
        if ( acceptAddress.empty() )
            acceptCond.wait ( mutex );
        address = acceptAddress;
        acceptAddress.clear();
        return address;
    }
};

class Client : public Socket
{
    CondVar connectCond;

protected:

    void tcpConnected ( const IpAddrPort& address )
    {
        connectCond.signal();
    }

    void tcpDisconnected ( const IpAddrPort& address )
    {
        LOG ( "Disconnected %s", address.c_str() );
    }

    void tcpReceived ( char *bytes, size_t len, const IpAddrPort& address )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), address.c_str() );
    }

    void udpReceived ( char *bytes, size_t len, const IpAddrPort& address )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), address.c_str() );
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
                IpAddrPort address = server->accept();

                LOG ( "Accepted %s", address.c_str() );

                server->tcpSend ( "Hi, I'm the server (TCP)", 24, address );

                Sleep ( 1000 );

                server->tcpDisconnect ( address );
            }
        }
        else if ( argc == 3 )
        {
            shared_ptr<Client> client ( new Client() );
            client->tcpConnect ( argv[1], atoi ( argv[2] ) );
            client->udpConnect ( argv[1], atoi ( argv[2] ) );

            // for ( ;; )
            {
                client->wait ( 5000 );

                if ( client->isConnected() )
                    LOG ( "Connected to %s%u", argv[1], atoi ( argv[2] ) );
                else
                    LOG ( "Connect failed" );

                client->tcpSend ( "Hi, I'm the client (TCP)", 24 );
                client->udpSend ( "Hi, I'm the client (UDP)", 24 );

                Sleep ( 5000 );

                client->tcpDisconnect();
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
