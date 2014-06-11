#include "EventManager.h"
#include "Socket.h"
#include "Log.h"

#include <cassert>

using namespace std;

#define LISTEN_INTERVAL     1000

static char socketReadBuffer[SOCKET_READ_BUFFER_SIZE];

EventManager::EventManager()
    : socketAcceptCmd ( socketMap )
    , socketDisconnectCmd ( socketMap )
    , socketReadCmd ( socketMap )
    , reaperThread ( zombieThreads )
{
    socketGroup.setCmdOnAccept ( &socketAcceptCmd );
    socketGroup.setCmdOnDisconnect ( &socketDisconnectCmd );
    socketGroup.setCmdOnRead ( &socketReadCmd );
}

EventManager::~EventManager()
{
}

void EventManager::addThread ( const shared_ptr<Thread>& thread )
{
    reaperThread.start();
    zombieThreads.push ( thread );
}

void EventManager::addSocket ( Socket *socket )
{
    assert ( socket->socket.get() );

    LOG ( "Adding socket %08x", socket );
    socketGroup.add ( socket->socket.get() );
    socketMap[socket->socket.get()] = socket;
}

void EventManager::removeSocket ( Socket *socket )
{
    assert ( socket->socket.get() );

    LOG ( "Removing socket %08x", socket );
    socketGroup.remove ( socket->socket.get() );
    socketMap.erase ( socket->socket.get() );
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

    assert ( address == IpAddrPort ( rawSocket ) );

    EventManager& em = EventManager::get();
    Lock lock ( em.mutex );

    LOG ( "Connected TCP socket; address='%s'", IpAddrPort ( rawSocket ).c_str() );
    if ( !rawSocket.get() )
    {
        socket->owner.disconnectEvent ( socket );
    }
    else
    {
        socket->socket = rawSocket;
        em.addSocket ( socket );
        socket->owner.connectEvent ( socket );
    }
}

void EventManager::connectTcpSocket ( Socket *socket, const IpAddrPort& address )
{
    LOG ( "Starting socket %08x connect thread; address='%s'", socket, address.c_str() );
    shared_ptr<Thread> thread ( new TcpConnectThread ( socket, address ) );
    thread->start();
    addThread ( thread );
}

void EventManager::start()
{
    LOG ( "Started listen loop" );

    for ( ;; )
    {
        LOCK ( mutex );
        try
        {
            socketGroup.listen ( LISTEN_INTERVAL );
        }
        catch ( const NL::Exception& e )
        {
            LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
            return;
        }
    }
}

void EventManager::ReaperThread::run()
{
    for ( ;; )
    {
        shared_ptr<Thread> thread = zombieThreads.pop();

        if ( thread )
            thread->join();
        else
            return;
    }
}

void EventManager::ReaperThread::join()
{
    zombieThreads.push ( shared_ptr<Thread>() );
    Thread::join();
}

void EventManager::SocketAccept::exec ( NL::Socket *serverSocket, NL::SocketGroup *, void * )
{
    auto it = socketMap.find ( serverSocket );
    assert ( it != socketMap.end() );

    LOG ( "Got accept for socket %08x", it->second );
    it->second->owner.acceptEvent ( it->second );
}

void EventManager::SocketDisconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    auto it = socketMap.find ( socket );
    assert ( it != socketMap.end() );

    LOG ( "Got disconnect for socket %08x", it->second );
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

    LOG ( "Got read for socket %08x; %u bytes from '%s'", len, address.c_str() );
    it->second->owner.readEvent ( it->second, socketReadBuffer, len, address );
}

EventManager& EventManager::get()
{
    static EventManager em;
    return em;
}
