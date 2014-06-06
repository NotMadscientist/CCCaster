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
    string acceptAddrPort;

protected:

    void tcpAccepted ( const std::string& addrPort )
    {
        acceptAddrPort = addrPort;
        acceptCond.signal();
    }

    void tcpDisconnected ( const std::string& addrPort )
    {
        LOG ( "Disconnected %s", addrPort.c_str() );
    }

    void tcpReceived ( char *bytes, size_t len, const std::string& addrPort )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), addrPort.c_str() );
    }

    void udpReceived ( char *bytes, size_t len, const string& addr, unsigned port )
    {
        LOG ( "Received '%s' from '%s:%u'", string ( bytes, len ).c_str(), addr.c_str(), port );
    }

public:

    string accept()
    {
        string addrPort;
        LOCK ( mutex );
        if ( acceptAddrPort.empty() )
            acceptCond.wait ( mutex );
        addrPort = acceptAddrPort;
        acceptAddrPort.clear();
        return addrPort;
    }
};

class Client : public Socket
{
    CondVar connectCond;

protected:

    void tcpConnected ( const std::string& addrPort )
    {
        connectCond.signal();
    }

    void tcpDisconnected ( const std::string& addrPort )
    {
        LOG ( "Disconnected %s", addrPort.c_str() );
    }

    void tcpReceived ( char *bytes, size_t len, const std::string& addrPort )
    {
        LOG ( "Received '%s' from '%s'", string ( bytes, len ).c_str(), addrPort.c_str() );
    }

    void udpReceived ( char *bytes, size_t len, const string& addr, unsigned port )
    {
        LOG ( "Received '%s' from '%s:%u'", string ( bytes, len ).c_str(), addr.c_str(), port );
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
                string addrPort = server->accept();

                LOG ( "Accepted %s", addrPort.c_str() );

                server->tcpSend ( "Hi, I'm the server (TCP)", 24, addrPort );

                Sleep ( 1000 );

                server->disconnect ( addrPort );
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

                client->disconnect();
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
