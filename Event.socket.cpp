#include "Event.h"
#include "Socket.h"
#include "Log.h"

#include <cassert>

using namespace std;

#define LISTEN_INTERVAL     1000

#define UDP_BIND_ATTEMPTS   10

#define PORT_MIN            4096
#define PORT_MAX            65535

#define RANDOM_PORT         ( PORT_MIN + rand() % ( PORT_MAX - PORT_MIN ) )

#define READ_BUFFER_SIZE    ( 1024 * 4096 )

static char socketReadBuffer[READ_BUFFER_SIZE];

void EventManager::addSocket ( Socket *socket )
{
    if ( socket->socket )
    {
        LOG ( "Adding %s socket %08x ( %08x ); address='%s'",
              socket->protocol == Socket::TCP ? "TCP" : "UDP", socket, socket->socket, socket->address.c_str() );

        socketMap[socket->socket] = socket;
        rawSocketMap[socket->socket] = shared_ptr<NL::Socket> ( socket->socket );
        rawSocketsToAdd.push_back ( socket->socket );

        socketsCond.signal();
    }
    else if ( socket->address.addr.empty() )
    {
        shared_ptr<NL::Socket> rawSocket (
            new NL::Socket ( socket->address.port, socket->protocol == Socket::TCP ? NL::TCP : NL::UDP, NL::IP4 ) );

        LOG ( "Opening %s socket %08x ( %08x ); port=%u",
              socket->protocol == Socket::TCP ? "TCP" : "UDP", socket, rawSocket.get(), socket->address.port );

        socket->socket = rawSocket.get();
        socketMap[rawSocket.get()] = socket;
        rawSocketMap[rawSocket.get()] = rawSocket;
        rawSocketsToAdd.push_back ( rawSocket.get() );

        socketsCond.signal();
    }
    else if ( socket->protocol == Socket::TCP )
    {
        LOG ( "Connecting TCP socket %08x; address='%s'", socket, socket->address.c_str() );

        shared_ptr<Thread> thread ( new TcpConnectThread ( socket ) );
        thread->start();
        addThread ( thread );
    }
    else
    {
        shared_ptr<NL::Socket> rawSocket;

        for ( int i = 0; i < UDP_BIND_ATTEMPTS; ++i )
        {
            unsigned localPort = RANDOM_PORT;

            try
            {
                rawSocket.reset ( new NL::Socket ( socket->address.addr, socket->address.port, localPort, NL::IP4 ) );
                break;
            }
            catch ( ... )
            {
                LOG ( "Failed to bind local UDP port %u", localPort );

                if ( i + 1 == UDP_BIND_ATTEMPTS )
                    throw;
                else
                    continue;
            }
        }

        LOG ( "Connecting UDP socket %08x ( %08x ); address='%s'", socket, rawSocket.get(), socket->address.c_str() );

        socket->socket = rawSocket.get();
        socketMap[rawSocket.get()] = socket;
        rawSocketMap[rawSocket.get()] = rawSocket;
        rawSocketsToAdd.push_back ( rawSocket.get() );

        socketsCond.signal();
    }
}

void EventManager::removeSocket ( Socket *socket )
{
    if ( !socket->socket )
        return;

    LOG ( "Removing socket %08x ( %08x )", socket, socket->socket );
    rawSocketsToRemove.push_back ( socket->socket );
    socket->socket = 0;
}

void EventManager::TcpConnectThread::run()
{
    shared_ptr<NL::Socket> rawSocket;
    try
    {
        rawSocket.reset ( new NL::Socket ( socket->address.addr, socket->address.port, NL::TCP, NL::IP4 ) );
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    if ( !rawSocket.get() )
    {
        LOG ( "Failed to connect TCP socket %08x; address='%s'", socket, socket->address.c_str() );
        return;
    }

    assert ( socket->address == IpAddrPort ( rawSocket ) );

    EventManager& em = EventManager::get();
    Lock lock ( em.mutex );

    socket->socket = rawSocket.get();
    em.socketMap[rawSocket.get()] = socket;
    em.rawSocketMap[rawSocket.get()] = rawSocket;
    em.rawSocketsToAdd.push_back ( rawSocket.get() );

    em.socketsCond.signal();

    LOG ( "Connected TCP socket %08x ( %08x ); address='%s'", socket, rawSocket.get(), socket->address.c_str() );
    socket->owner.connectEvent ( socket );
}

void EventManager::socketListenLoop()
{
    running = true;

    for ( ;; )
    {
        LOCK ( mutex );

        for ( ;; )
        {
            if ( socketGroup.empty() && rawSocketsToAdd.empty() )
            {
                LOG ( "Waiting for sockets..." );
                socketsCond.wait ( mutex );

                if ( !running )
                    return;
            }

            for ( NL::Socket *rawSocket : rawSocketsToAdd )
            {
                LOG ( "Added socket %08x ( %08x )", socketMap[rawSocket], rawSocket );
                socketGroup.add ( rawSocket );
            }

            rawSocketsToAdd.clear();

            for ( NL::Socket *rawSocket : rawSocketsToRemove )
            {
                LOG ( "Removed socket %08x ( %08x )", socketMap[rawSocket], rawSocket );
                socketGroup.remove ( rawSocket );
                socketMap.erase ( rawSocket );
                rawSocketMap.erase ( rawSocket );
            }

            rawSocketsToRemove.clear();

            if ( !socketGroup.empty() )
                break;
        }

        LOG ( "Listening to %u socket(s)...", socketGroup.size() );

        try
        {
            socketGroup.listen ( LISTEN_INTERVAL );
        }
        catch ( const NL::Exception& e )
        {
            LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
            break;
        }

        if ( !running )
            return;
    }
}

void EventManager::SocketAccept::exec ( NL::Socket *serverSocket, NL::SocketGroup *, void * )
{
    auto it = socketMap.find ( serverSocket );
    assert ( it != socketMap.end() );

    LOG ( "serverSocket %08x ( %08x )", it->second, it->second->socket );
    it->second->owner.acceptEvent ( it->second );
}

void EventManager::SocketDisconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    auto it = socketMap.find ( socket );
    assert ( it != socketMap.end() );

    if ( !it->second->socket )
        return;

    LOG ( "socket %08x ( %08x )", it->second, it->second->socket );
    it->second->owner.disconnectEvent ( it->second );
    it->second->disconnect();
}

void EventManager::SocketRead::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    auto it = socketMap.find ( socket );
    assert ( it != socketMap.end() );

    IpAddrPort address ( socket );
    size_t len;

    if ( socket->protocol() == NL::TCP )
        len = socket->read ( socketReadBuffer, sizeof ( socketReadBuffer ) );
    else
        len = socket->readFrom ( socketReadBuffer, sizeof ( socketReadBuffer ), &address.addr, &address.port );

    LOG ( "socket %08x ( %08x ); %u bytes from '%s'",
          it->second, it->second->socket, len, address.c_str() );
    it->second->owner.readEvent ( it->second, socketReadBuffer, len, address );
}
