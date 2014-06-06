#include "Socket.h"
#include "Log.h"

#include <sstream>
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
    string addrPort = getAddrPort ( socket );

    LOG ( "addrPort='%s'", addrPort.c_str() );

    Lock lock ( context.mutex );

    context.acceptedSockets[addrPort] = shared_ptr<NL::Socket> ( socket );
    context.socketGroup->add ( socket );

    LOG ( "Socket::tcpAccepted ( %s )", addrPort.c_str() );
    context.tcpAccepted ( addrPort );
}

void Socket::Disconnect::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    string addrPort = getAddrPort ( socket );

    LOG ( "addrPort='%s'", addrPort.c_str() );

    Lock lock ( context.mutex );

    context.socketGroup->remove ( socket );
    context.acceptedSockets.erase ( addrPort );
    if ( context.tcpSocket.get() == socket )
        context.tcpSocket.reset();

    LOG ( "Socket::tcpDisconnected ( %s )", addrPort.c_str() );
    context.tcpDisconnected ( addrPort );
}

void Socket::Read::exec ( NL::Socket *socket, NL::SocketGroup *, void * )
{
    string addrPort = getAddrPort ( socket );

    LOG ( "addrPort='%s'", addrPort.c_str() );

    Lock lock ( context.mutex );

    size_t len;

    if ( socket->protocol() == NL::TCP )
    {
        len = socket->read ( context.readBuffer, sizeof ( context.readBuffer ) );

        LOG ( "Socket::tcpReceived ( [%u bytes], '%s' )", len, addrPort.c_str() );
        context.tcpReceived ( context.readBuffer, len, addrPort );
    }
    else
    {
        string addr;
        unsigned port;
        len = socket->readFrom ( context.readBuffer, sizeof ( context.readBuffer ), &addr, &port );

        LOG ( "Socket::udpReceived ( [%u bytes], '%s:%u' )", len, addr.c_str(), port );
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

    if ( !socket.get() )
        return;
    string addrPort = getAddrPort ( socket );

    Lock lock ( context.mutex );
    if ( !context.tcpSocket )
    {
        context.tcpSocket = socket;
        context.addSocketToGroup ( socket );
    }

    LOG ( "Socket::tcpConnected ( %s )", addrPort.c_str() );
    context.tcpConnected ( addrPort );
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
    LOG ( "protocol=%s, local='%s:%u', remote='%s:%u'",
          socket->protocol() == NL::TCP ? "TCP" : "UDP",
          socket->hostFrom().c_str(), socket->portFrom(), socket->hostTo().c_str(), socket->portTo() );

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
    LOG ( "port=%u", port );

    serverSocket.reset ( new NL::Socket ( port, NL::TCP, NL::IP4 ) );
    udpSocket.reset ( new NL::Socket ( port, NL::UDP, NL::IP4 ) );

    addSocketToGroup ( serverSocket );
    addSocketToGroup ( udpSocket );
}

void Socket::tcpConnect ( const string& addr, unsigned port )
{
    LOG ( "addr='%s', port=%u", addr.c_str(), port );

    shared_ptr<ConnectThread> connectThread ( new ConnectThread ( *this, addr, port ) );
    connectThread->start();
    connectingThreads.push ( connectThread );
}

void Socket::udpConnect ( const string& addr, unsigned port )
{
    LOG ( "addr='%s', port=%u", addr.c_str(), port );

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
}

void Socket::disconnect ( const string& addrPort )
{
    LOG ( "addrPort='%s'", addrPort.c_str() );

    LOCK ( mutex );

    if ( addrPort.empty() || tcpSocket.get() )
    {
        LOG ( "ListenThread::join()" );
        listenThread.join();

        LOG ( "Closing all sockets" );
        acceptedSockets.clear();
        socketGroup.reset();
        udpSocket.reset();
        tcpSocket.reset();
        serverSocket.reset();
    }
    else
    {
        auto it = acceptedSockets.find ( addrPort );
        if ( it != acceptedSockets.end() && it->second )
        {
            socketGroup->remove ( it->second.get() );
            acceptedSockets.erase ( it );
        }
        else if ( addrPort == getAddrPort ( udpSocket ) )
        {
            udpSocket.reset();
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

void Socket::tcpSend ( char *bytes, size_t len, const string& addrPort )
{
    LOCK ( mutex );

    if ( addrPort.empty() && tcpSocket.get() )
    {
        tcpSocket->send ( bytes, len );
    }
    else
    {
        auto it = acceptedSockets.find ( addrPort );
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

string Socket::getAddrPort ( const shared_ptr<NL::Socket>& socket )
{
    return getAddrPort ( socket.get() );
}

string Socket::getAddrPort ( const NL::Socket *socket )
{
    if ( !socket )
        return "";
    stringstream ss;
    ss << socket->hostTo() << ':' << socket->portTo();
    return ss.str();
}

string Socket::getAddrPort ( const string& addr, unsigned port )
{
    if ( addr.empty() || !port )
        return "";
    stringstream ss;
    ss << addr << ':' << port;
    return ss.str();
}
