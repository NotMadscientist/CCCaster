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

#define LOG_SOCKET(VERB, SOCKET)                                                                        \
    LOG ( "%s %s socket %08x { %08x, '%s' }",                                                           \
          VERB, TO_C_STR ( SOCKET->protocol ), SOCKET, SOCKET->socket.get(), SOCKET->address.c_str() )

void EventManager::addSocket ( Socket *socket )
{
    LOCK ( mutex );

    if ( socket->socket.get() ) // Add socket from accept
    {
        LOG_SOCKET ( "Adding", socket );

        assert ( socket->address == IpAddrPort ( socket->socket ) );

        activeSockets.insert ( socket );
        socketsCond.signal();
    }
    else if ( socket->address.addr.empty() ) // Add socket from listen
    {
        shared_ptr<NL::Socket> rawSocket (
            new NL::Socket ( socket->address.port, socket->protocol == Protocol::TCP ? NL::TCP : NL::UDP, NL::IP4 ) );

        LOG_SOCKET ( "Opening", socket );

        assert ( socket->address.port == rawSocket->portFrom() );

        socket->socket = rawSocket;
        activeSockets.insert ( socket );
        socketsCond.signal();
    }
    else if ( socket->protocol == Protocol::TCP ) // Add socket from TCP connect
    {
        LOG_SOCKET ( "Connecting", socket );

        connectingSockets.insert ( socket );

        shared_ptr<Thread> thread ( new TcpConnectThread ( socket, socket->address ) );
        thread->start();
        addThread ( thread );
    }
    else // Add socket from UDP connect
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

        LOG_SOCKET ( "Connected", socket );

        assert ( socket->address == IpAddrPort ( rawSocket ) );

        socket->socket = rawSocket;
        activeSockets.insert ( socket );
        socketsCond.signal();
    }
}

void EventManager::removeSocket ( Socket *socket )
{
    LOCK ( mutex );

    if ( !activeSockets.erase ( socket ) && !connectingSockets.erase ( socket ) )
        return;

    LOG_SOCKET ( "Removing", socket );
    socket->socket.reset();
    socketsCond.signal();
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
        em.connectingSockets.erase ( socket );
        return;
    }

    if ( em.connectingSockets.find ( socket ) == em.connectingSockets.end() )
        return;

    assert ( socket->address == IpAddrPort ( rawSocket ) );

    socket->socket = rawSocket;
    em.connectingSockets.erase ( socket );
    em.activeSockets.insert ( socket );
    em.socketsCond.signal();

    LOG_SOCKET ( "Connected", socket );
    socket->owner->connectEvent ( socket );
}

void EventManager::socketListenLoop()
{
    for ( ;; )
    {
        Sleep ( 1 );

        LOCK ( mutex );

        for ( ;; )
        {
            if ( socketGroup.empty() && activeSockets.empty() )
            {
                LOG ( "Waiting for sockets..." );
                socketsCond.wait ( mutex );

                if ( !running )
                    break;
            }

            if ( !running )
                break;

            for ( Socket *socket : activeSockets )
            {
                if ( activeRawSockets.find ( socket ) != activeRawSockets.end() )
                    continue;

                LOG ( "Added %s socket %08x { %08x, '%s' }",
                      TO_C_STR ( socket->protocol ), socket, socket->socket.get(), socket->address.c_str() );

                socketGroup.add ( socket->socket.get() );
                rawSocketToSocket[socket->socket.get()] = socket;
                activeRawSockets[socket] = socket->socket;
            }

            auto it = activeRawSockets.begin();
            while ( it != activeRawSockets.end() )
            {
                if ( activeSockets.find ( it->first ) != activeSockets.end() )
                {
                    ++it;
                    continue;
                }

                LOG ( "Removed %s socket %08x { %08x, '%s:%u' }",
                      TO_C_STR ( it->second->protocol() ), it->first, it->second.get(), it->second->hostTo().c_str(),
                      ( it->second->type() == NL::SERVER ? it->second->portFrom() : it->second->portTo() ) );

                socketGroup.remove ( it->second.get() );
                rawSocketToSocket.erase ( it->second.get() );
                activeRawSockets.erase ( it++ );
            }

            if ( !socketGroup.empty() )
                break;
        }

        if ( !running )
            break;

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
            break;
    }

    socketGroup.clear();
    rawSocketToSocket.clear();
    connectingSockets.clear();
    activeRawSockets.clear();
}

void EventManager::SocketAccept::exec ( NL::Socket *serverSocket, NL::SocketGroup *, void * )
{
    EventManager& em = EventManager::get();

    auto it = em.rawSocketToSocket.find ( serverSocket );
    assert ( it != em.rawSocketToSocket.end() );

    if ( em.activeSockets.find ( it->second ) == em.activeSockets.end()
            || !it->second->owner || !it->second->socket.get() )
        return;

    LOG_SOCKET ( "Accept from server", it->second );

    NL::Socket *rawSocket = serverSocket->accept();
    assert ( rawSocket );
    it->second->acceptedSocket.reset ( new Socket ( rawSocket ) );

    it->second->owner->acceptEvent ( it->second );
    it->second->acceptedSocket.reset();
}

void EventManager::SocketDisconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    EventManager& em = EventManager::get();

    auto it = em.rawSocketToSocket.find ( socket );
    assert ( it != em.rawSocketToSocket.end() );

    if ( em.activeSockets.find ( it->second ) == em.activeSockets.end()
            || !it->second->owner || !it->second->socket.get() )
        return;

    LOG_SOCKET ( "Disconnected", it->second );
    it->second->owner->disconnectEvent ( it->second );
    it->second->disconnect();
}

void EventManager::SocketRead::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    EventManager& em = EventManager::get();

    auto it = em.rawSocketToSocket.find ( socket );
    assert ( it != em.rawSocketToSocket.end() );

    if ( em.activeSockets.find ( it->second ) == em.activeSockets.end()
            || !it->second->owner || !it->second->socket.get() )
        return;

    char *buffer = & ( it->second->readBuffer[it->second->readPos] );
    size_t bufferSize = it->second->readBuffer.size() - it->second->readPos;

    IpAddrPort address ( socket );
    size_t len, consumed;

    if ( socket->protocol() == NL::TCP )
        len = socket->read ( buffer, bufferSize );
    else
        len = socket->readFrom ( buffer, bufferSize, & ( address.addr ), & ( address.port ) );

    if ( len == 0 )
        return;

    it->second->readPos += len;

    LOG_SOCKET ( "Read from", it->second );
    LOG ( "Read [ %u bytes ] from '%s'", len, address.c_str() );

    MsgPtr msg = Serializable::decode ( buffer, it->second->readPos, consumed );

    assert ( consumed <= it->second->readPos );

    it->second->readBuffer.erase ( 0, consumed );
    it->second->readPos -= consumed;

    if ( msg.get() )
    {
        LOG ( "Decoded [ %u bytes ] to '%s'", len, TO_C_STR ( msg ) );
        it->second->owner->readEvent ( it->second, msg, address );
    }
}
