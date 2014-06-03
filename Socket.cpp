#include "Socket.h"

using namespace std;

#define LISTEN_INTERVAL 1000

void Socket::Accept::exec ( NL::Socket *, NL::SocketGroup *, void * )
{
    Lock lock ( context.mutex );
}

void Socket::Disconnect::exec ( NL::Socket *, NL::SocketGroup *, void * )
{
    Lock lock ( context.mutex );
}

void Socket::Read::exec ( NL::Socket *, NL::SocketGroup *, void * )
{
    Lock lock ( context.mutex );
}

void Socket::ListenThread::run()
{
    for ( ;; )
    {
        try
        {
            context.socketGroup->listen ( LISTEN_INTERVAL, &context );
        }
        catch ( const NL::Exception& e )
        {
            // LOG ( "socketGroup::listen exception: [%d] %s", e.nativeErrorCode(), e.what() );
            break;
        }
    }
}

void Socket::listen ( int port )
{
    socketGroup.reset ( new NL::SocketGroup() );
    socketGroup->setCmdOnAccept ( &socketAccept );
    socketGroup->setCmdOnDisconnect ( &socketDisconnect );
    socketGroup->setCmdOnRead ( &socketRead );

    serverSocket.reset ( new NL::Socket ( port ) );
    udpSocket.reset ( new NL::Socket ( port, NL::UDP ) );

    socketGroup->add ( serverSocket.get() );
    socketGroup->add ( udpSocket.get() );

    listenThread.start();
}

void Socket::connect ( string addr, int port )
{
    // connectThread.start();
}

void Socket::disconnect()
{
}

bool Socket::isConnected() const
{
    return false;
}

string Socket::localAddr() const
{
    return "";
}

string Socket::remoteAddr ( uint32_t id ) const
{
    return "";
}

void Socket::send ( uint32_t id, char *bytes, size_t len )
{
}
