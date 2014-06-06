#include "Socket.h"
#include "Log.h"

#include <cstdlib>

using namespace std;

#define LISTEN_INTERVAL     1000

#define UDP_BIND_ATTEMPTS   10

#define PORT_MIN            4096
#define PORT_MAX            65535

#define RANDOM_PORT         ( PORT_MIN + rand() % ( PORT_MAX - PORT_MIN ) )

BlockingQueue<shared_ptr<Socket::ConnectThread>> Socket::connectingThreads;

Socket::ReaperThread Socket::reaperThread;

Socket::Socket()
    : socketAcceptCmd ( *this )
    , socketDisconnectCmd ( *this )
    , socketReadCmd ( *this )
    , listenThread ( *this )
{
}

Socket::~Socket()
{
    disconnect();
}

void Socket::Accept::exec ( NL::Socket *serverSocket, NL::SocketGroup *, void * )
{
    NL::Socket *socket = serverSocket->accept();
    uint32_t id = ( uint32_t ) socket;

    LOG ( "id=%08x", id );

    Lock lock ( context.mutex );

    context.acceptedSockets[id] = shared_ptr<NL::Socket> ( socket );
    context.socketGroup->add ( socket );

    LOG ( "Socket::tcpAccepted ( %08x )", id );
    context.tcpAccepted ( id );
}

void Socket::Disconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    uint32_t id = ( uint32_t ) socket;

    LOG ( "id=%08x", id );

    Lock lock ( context.mutex );

    context.socketGroup->remove ( socket );
    context.acceptedSockets.erase ( id );
    if ( context.tcpSocket.get() == socket )
        context.tcpSocket.reset();

    LOG ( "Socket::tcpDisconnected ( %08x )", id );
    context.tcpDisconnected ( id );
}

void Socket::Read::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    uint32_t id = ( uint32_t ) socket;

    LOG ( "id=%08x", id );

    Lock lock ( context.mutex );

    size_t len;

    if ( socket->protocol() == NL::TCP )
    {
        len = socket->read ( context.readBuffer, sizeof ( context.readBuffer ) );

        LOG ( "Socket::tcpReceived ( [%u bytes], %08x )", len, id );
        context.tcpReceived ( context.readBuffer, len, id );
    }
    else
    {
        string addr;
        unsigned port;
        len = socket->readFrom ( context.readBuffer, sizeof ( context.readBuffer ), &addr, &port );

        LOG ( "Socket::udpReceived ( [%u bytes], %s:%u )", len, addr.c_str(), port );
        context.udpReceived ( context.readBuffer, len, addr, port );
    }
}

void Socket::ListenThread::start()
{
    if ( isListening )
        return;

    isListening = true;
    Thread::start();
}

void Socket::ListenThread::join()
{
    if ( !isListening )
        return;

    isListening = false;
    Thread::join();
}

void Socket::ListenThread::run()
{
    while ( isListening )
    {
        try
        {
            context.socketGroup->listen ( LISTEN_INTERVAL );
        }
        catch ( const NL::Exception& e )
        {
            LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
            break;
        }
    }
}

void Socket::ConnectThread::start()
{
    reaperThread.start();
    Thread::start();
}

void Socket::ConnectThread::run()
{
    shared_ptr<NL::Socket> socket;
    try
    {
        socket.reset ( new NL::Socket ( addr, port, NL::TCP, NL::IP4 ) );
    }
    catch ( const NL::Exception& e )
    {
        LOG ( "[%d] %s", e.nativeErrorCode(), e.what() );
    }

    uint32_t id = ( uint32_t ) socket.get();
    if ( !id )
        return;

    Lock lock ( context.mutex );
    if ( !context.tcpSocket )
    {
        context.tcpSocket = socket;
        context.addSocketToGroup ( socket );
    }

    LOG ( "Socket::tcpConnected ( %08x )", id );
    context.tcpConnected ( id );
}

void Socket::ReaperThread::run()
{
    for ( ;; )
    {
        shared_ptr<ConnectThread> connectThread = connectingThreads.pop();

        if ( connectThread )
            connectThread->join();
        else
            break;
    }
}

void Socket::ReaperThread::join()
{
    connectingThreads.push ( shared_ptr<ConnectThread>() );
    Thread::join();
}

void Socket::addSocketToGroup ( const shared_ptr<NL::Socket>& socket )
{
    LOG ( "id=%08x", ( uint32_t ) socket.get() );

    if ( !socketGroup )
    {
        socketGroup.reset ( new NL::SocketGroup() );
        socketGroup->setCmdOnAccept ( &socketAcceptCmd );
        socketGroup->setCmdOnDisconnect ( &socketDisconnectCmd );
        socketGroup->setCmdOnRead ( &socketReadCmd );
    }

    socketGroup->add ( socket.get() );

    listenThread.start();
}

void Socket::listen ( unsigned port )
{
    LOG ( "port=%d", port );

    serverSocket.reset ( new NL::Socket ( port, NL::TCP, NL::IP4 ) );
    udpSocket.reset ( new NL::Socket ( port, NL::UDP, NL::IP4 ) );

    addSocketToGroup ( serverSocket );
    addSocketToGroup ( udpSocket );
}

void Socket::connect ( string addr, unsigned port )
{
    LOG ( "addr=%s, port=%d", addr.c_str(), port );

    for ( int i = 0; i < UDP_BIND_ATTEMPTS; ++i )
    {
        try
        {
            udpSocket.reset ( new NL::Socket ( addr, port, RANDOM_PORT, NL::IP4 ) );
            break;
        }
        catch ( ... )
        {
            if ( i + 1 == UDP_BIND_ATTEMPTS )
                throw;
            else
                continue;
        }
    }

    addSocketToGroup ( udpSocket );

    shared_ptr<ConnectThread> connectThread ( new ConnectThread ( *this, addr, port ) );
    connectThread->start();
    connectingThreads.push ( connectThread );
}

void Socket::disconnect ( uint32_t id )
{
    LOG ( "id=%08x", id );

    LOCK ( mutex );

    if ( id == 0 || tcpSocket.get() )
    {
        listenThread.join();

        acceptedSockets.clear();
        socketGroup.reset();
        udpSocket.reset();
        tcpSocket.reset();
        serverSocket.reset();
    }
    else
    {
        auto it = acceptedSockets.find ( id );
        if ( it != acceptedSockets.end() && it->second )
        {
            socketGroup->remove ( it->second.get() );
            acceptedSockets.erase ( it );
        }
    }
}

bool Socket::isServer() const
{
    LOCK ( mutex );
    return ( serverSocket.get() && !tcpSocket );
}

bool Socket::isConnected() const
{
    LOCK ( mutex );
    return tcpSocket.get();
}

string Socket::localAddr() const
{
    LOCK ( mutex );

    if ( tcpSocket )
        return tcpSocket->hostFrom();

    if ( serverSocket )
        return serverSocket->hostTo();

    return "";
}

string Socket::remoteAddr ( uint32_t id ) const
{
    LOCK ( mutex );

    if ( tcpSocket )
        return tcpSocket->hostTo();

    auto it = acceptedSockets.find ( id );
    if ( it != acceptedSockets.end() && it->second )
        return it->second->hostTo();

    return "";
}

void Socket::tcpSend ( char *bytes, size_t len, uint32_t id )
{
    LOCK ( mutex );

    if ( id == 0 || tcpSocket.get() )
    {
        tcpSocket->send ( bytes, len );
    }
    else
    {
        auto it = acceptedSockets.find ( id );
        if ( it != acceptedSockets.end() && it->second )
            it->second->send ( bytes, len );
    }
}

void Socket::udpSend ( char *bytes, size_t len, const string& addr, unsigned port )
{
    LOCK ( mutex );

    if ( udpSocket.get() )
    {
        if ( addr.empty() || !port )
            udpSocket->send ( bytes, len );
        else
            udpSocket->sendTo ( bytes, len, addr, port );
    }
}

void Socket::release()
{
    LOG ( "ReaperThread::release()" );
    reaperThread.release();
}
