#include "Event.h"
#include "Socket.h"
#include "Log.h"

#include <cassert>

using namespace std;

#define LISTEN_INTERVAL     100

#define UDP_BIND_ATTEMPTS   10

#define PORT_MIN            4096
#define PORT_MAX            65535

#define RANDOM_PORT         ( PORT_MIN + rand() % ( PORT_MAX - PORT_MIN ) )

#define READ_BUFFER_SIZE    ( 1024 * 4096 )

#define LOG_SOCKET(VERB, SOCKET)                                                                        \
    LOG ( "%s %s socket %08x ( %08x, '%s' )",                                                           \
          VERB, TO_C_STR ( SOCKET->protocol ), SOCKET, SOCKET->socket, SOCKET->address.c_str() )

static char socketReadBuffer[READ_BUFFER_SIZE];

void EventManager::addSocket ( Socket *socket )
{
    LOCK ( mutex );

    assert ( socketSet.find ( socket ) == socketSet.end() );

    socketSet.insert ( socket );

    if ( socket->socket )
    {
        LOG_SOCKET ( "Adding", socket );

        assert ( socket->address == IpAddrPort ( socket->socket ) );

        socketMap[socket->socket] = socket;
        rawSocketMap[socket->socket] = shared_ptr<NL::Socket> ( socket->socket );
        rawSocketsToAdd.push_back ( socket->socket );

        socketsCond.signal();
    }
    else if ( socket->address.addr.empty() )
    {
        shared_ptr<NL::Socket> rawSocket (
            new NL::Socket ( socket->address.port, socket->protocol == Protocol::TCP ? NL::TCP : NL::UDP, NL::IP4 ) );

        LOG_SOCKET ( "Opening", socket );

        assert ( socket->address.port == rawSocket->portFrom() );

        socket->socket = rawSocket.get();

        socketMap[rawSocket.get()] = socket;
        rawSocketMap[rawSocket.get()] = rawSocket;
        rawSocketsToAdd.push_back ( rawSocket.get() );

        socketsCond.signal();
    }
    else if ( socket->protocol == Protocol::TCP )
    {
        LOG_SOCKET ( "Connecting", socket );

        shared_ptr<Thread> thread ( new TcpConnectThread ( socket, socket->address ) );
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

        LOG_SOCKET ( "Connecting", socket );

        assert ( socket->address == IpAddrPort ( rawSocket ) );

        socket->socket = rawSocket.get();

        socketMap[rawSocket.get()] = socket;
        rawSocketMap[rawSocket.get()] = rawSocket;
        rawSocketsToAdd.push_back ( rawSocket.get() );

        socketsCond.signal();
    }
}

void EventManager::removeSocket ( Socket *socket )
{
    LOCK ( mutex );

    if ( !socketSet.erase ( socket ) )
        return;

    LOG_SOCKET ( "Removing", socket );

    rawSocketsToRemove.push_back ( socket->socket );
    socket->socket = 0;
}

void EventManager::TcpConnectThread::run()
{
    shared_ptr<NL::Socket> rawSocket;
    try
    {
        rawSocket.reset ( new NL::Socket ( address.addr, address.port, NL::TCP, NL::IP4 ) );
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    EventManager& em = EventManager::get();
    Lock lock ( em.mutex );

    if ( !rawSocket.get() )
    {
        LOG_SOCKET ( "Failed to connect", socket );
        em.socketSet.erase ( socket );
        return;
    }

    if ( em.socketSet.find ( socket ) == em.socketSet.end() )
        return;

    assert ( socket->address == IpAddrPort ( rawSocket ) );

    socket->socket = rawSocket.get();

    em.socketMap[rawSocket.get()] = socket;
    em.rawSocketMap[rawSocket.get()] = rawSocket;
    em.rawSocketsToAdd.push_back ( rawSocket.get() );

    em.socketsCond.signal();

    LOG_SOCKET ( "Connected", socket );
    socket->owner.connectEvent ( socket );
}

void EventManager::socketListenLoop()
{
    running = true;

    for ( ;; )
    {
        Sleep ( 1 );

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
                LOG_SOCKET ( "Added", socketMap[rawSocket] );
                socketGroup.add ( rawSocket );
            }

            rawSocketsToAdd.clear();

            for ( NL::Socket *rawSocket : rawSocketsToRemove )
            {
                LOG_SOCKET ( "Removed", socketMap[rawSocket] );
                socketGroup.remove ( rawSocket );
                socketMap.erase ( rawSocket );
                rawSocketMap.erase ( rawSocket );
            }

            rawSocketsToRemove.clear();

            if ( !socketGroup.empty() )
                break;
        }

        // LOG ( "Listening to %u socket(s)...", socketGroup.size() );

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
    EventManager& em = EventManager::get();

    auto it = em.socketMap.find ( serverSocket );
    assert ( it != em.socketMap.end() );

    if ( em.socketSet.find ( it->second ) == em.socketSet.end() )
        return;

    if ( !it->second->socket )
        return;

    LOG_SOCKET ( "Accept from server", it->second );
    it->second->owner.acceptEvent ( it->second );
}

void EventManager::SocketDisconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    EventManager& em = EventManager::get();

    auto it = em.socketMap.find ( socket );
    assert ( it != em.socketMap.end() );

    if ( em.socketSet.find ( it->second ) == em.socketSet.end() )
        return;

    if ( !it->second->socket )
        return;

    LOG_SOCKET ( "Disconnected", it->second );
    it->second->owner.disconnectEvent ( it->second );
    it->second->disconnect();
}

void EventManager::SocketRead::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    EventManager& em = EventManager::get();

    auto it = em.socketMap.find ( socket );
    assert ( it != em.socketMap.end() );

    if ( em.socketSet.find ( it->second ) == em.socketSet.end() )
        return;

    if ( !it->second->socket )
        return;

    IpAddrPort address ( socket );
    size_t len;

    if ( socket->protocol() == NL::TCP )
        len = socket->read ( socketReadBuffer, sizeof ( socketReadBuffer ) );
    else
        len = socket->readFrom ( socketReadBuffer, sizeof ( socketReadBuffer ), &address.addr, &address.port );

    LOG_SOCKET ( "Read from", it->second );
    LOG ( "Read [ %u bytes ] from '%s'", len, address.c_str() );
    it->second->owner.readEvent ( it->second, socketReadBuffer, len, address );
}
