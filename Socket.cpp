#include "Event.h"
#include "Socket.h"
#include "Log.h"
#include "Util.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>

#define READ_BUFFER_SIZE ( 1024 * 4096 )

using namespace std;

Socket::Socket ( Owner *owner, int fd, const string& address, unsigned port )
    : fd ( fd ), owner ( owner ), state ( State::Connected ), address ( address, port ), protocol ( Protocol::TCP )
{
    assert ( this->fd != 0 );
    init();
}

Socket::Socket ( Owner *owner, unsigned port, Protocol protocol )
    : fd ( 0 ), owner ( owner ), state ( State::Listening ), address ( "", port ), protocol ( protocol )
{
    init();
}

Socket::Socket ( Owner *owner, const string& address, unsigned port, Protocol protocol )
    : fd ( 0 ), owner ( owner ), state ( State::Connecting ), address ( address, port ), protocol ( protocol )
{
    assert ( this->address.addr.empty() == false );
    init();
}

// Socket::Socket ( Owner *owner, const string& address, unsigned port )
//     : socket ( 0 ), protocol ( Protocol::UDP ), readBuffer ( READ_BUFFER_SIZE, ( char ) 0 )
//     , readPos ( 0 ), packetLoss ( 0 ), owner ( owner ), address ( address, port )
// {
// }

void Socket::init()
{
    if ( fd == 0 )
    {
        shared_ptr<addrinfo> addrInfo;

        if ( isClient() && protocol == Protocol::UDP )
        {
            state = State::Connected;
            addrInfo = IpAddrPort().updateAddrInfo ( true );
        }
        else
        {
            addrInfo = address.updateAddrInfo ( isServer() || protocol == Protocol::UDP );
        }

        addrinfo *res = addrInfo.get();

        for ( ; res; res = res->ai_next )
        {
            fd = ::socket ( res->ai_family,
                            ( protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM ),
                            ( protocol == Protocol::TCP ? IPPROTO_TCP : IPPROTO_UDP ) );

            if ( fd == INVALID_SOCKET )
            {
                LOG_SOCKET ( ( "socket failed: " + getLastWinSockError() ).c_str(), this );
                fd = 0;
                continue;
            }

            u_long nonBlocking = 1;

            if ( ioctlsocket ( fd, FIONBIO, &nonBlocking ) != 0 )
            {
                LOG_SOCKET ( ( "ioctlsocket failed: " + getLastWinSockError() ).c_str(), this );
                closesocket ( fd );
                fd = 0;
                throw "something"; // TODO
            }

            if ( isClient() )
            {
                if ( protocol == Protocol::TCP )
                {
                    if ( ::connect ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
                    {
                        int error = WSAGetLastError();

                        // Non-blocking connect
                        if ( error == WSAEWOULDBLOCK )
                            break;

                        LOG_SOCKET ( ( "connect failed: " + getWindowsErrorAsString ( error ) ).c_str(), this );
                        closesocket ( fd );
                        fd = 0;
                        continue;
                    }
                }
                else
                {
                    if ( ::bind ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
                    {
                        LOG_SOCKET ( ( "bind failed: " + getLastWinSockError() ).c_str(), this );
                        closesocket ( fd );
                        fd = 0;
                        continue;
                    }
                }
            }
            else
            {
                char yes = 1;

                // SO_REUSEADDR can replace existing binds
                // SO_EXCLUSIVEADDRUSE only replaces if not exact match
                if ( setsockopt ( fd, SOL_SOCKET, SO_REUSEADDR, &yes, 1 ) == SOCKET_ERROR )
                {
                    LOG_SOCKET ( ( "setsockopt failed: " + getLastWinSockError() ).c_str(), this );
                    closesocket ( fd );
                    fd = 0;
                    throw "something"; // TODO
                }

                if ( ::bind ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
                {
                    LOG_SOCKET ( ( "bind failed: " + getLastWinSockError() ).c_str(), this );
                    closesocket ( fd );
                    fd = 0;
                    continue;
                }

                if ( protocol == Protocol::TCP && ( ::listen ( fd, SOMAXCONN ) == SOCKET_ERROR ) )
                {
                    LOG_SOCKET ( ( "listen failed: " + getLastWinSockError() ).c_str(), this );
                    closesocket ( fd );
                    fd = 0;
                    throw "something"; // TODO
                }

                break;
            }
        }
    }

    if ( fd == 0 )
    {
        LOG_SOCKET ( "init failed", this );
        throw "something"; // TODO
    }

    readBuffer.resize ( READ_BUFFER_SIZE );
    readPos = packetLoss = 0;

    EventManager::get().addSocket ( this );
}

Socket::~Socket()
{
    disconnect();
}

void Socket::disconnect()
{
    if ( fd )
    {
        EventManager::get().removeSocket ( this );
        closesocket ( fd );
    }

    fd = readPos = packetLoss = 0;
    state = State::Disconnected;
    owner = 0;
    readBuffer.clear();
}

shared_ptr<Socket> Socket::listen ( Owner *owner, unsigned port, Protocol protocol )
{
    return shared_ptr<Socket> ( new Socket ( owner, port, protocol ) );
}

shared_ptr<Socket> Socket::connect ( Owner *owner, const string& address, unsigned port, Protocol protocol )
{
    return shared_ptr<Socket> ( new Socket ( owner, address, port, protocol ) );
}

shared_ptr<Socket> Socket::accept ( Owner *owner )
{
    if ( !isServer() )
        return 0;

    sockaddr_storage sa;
    int saLen = sizeof ( sa );

    int newFd = ::accept ( fd, ( struct sockaddr * ) &sa, &saLen );

    if ( newFd == INVALID_SOCKET )
    {
        LOG_SOCKET ( ( "accept failed: " + getLastWinSockError() ).c_str(), this );
        return 0;
    }

    if ( sa.ss_family != AF_INET )
    {
        closesocket ( newFd );
        LOG_SOCKET ( "Only accepting IPv4 from", this );
        return 0;
    }

    // TODO IPv6
    char newAddr[INET6_ADDRSTRLEN];
    inet_ntop ( sa.ss_family, & ( ( ( struct sockaddr_in * ) &sa )->sin_addr ), newAddr, sizeof ( newAddr ) );

    unsigned newPort = ntohs ( ( ( struct sockaddr_in * ) &sa )->sin_port );

    return shared_ptr<Socket> ( new Socket ( owner, newFd, newAddr, newPort ) );
}

bool Socket::send ( Serializable *message, const IpAddrPort& address )
{
    MsgPtr msg ( message );
    return send ( msg, address );
}

bool Socket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    string buffer = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", TO_C_STR ( msg ), buffer.size() );
    LOG ( "Base64 : %s", toBase64 ( buffer ).c_str() );

    return send ( &buffer[0], buffer.size(), address );
}

bool Socket::send ( const char *buffer, size_t len, const IpAddrPort& address )
{
    if ( fd != 0 )
    {
        size_t totalBytes = 0;

        while ( totalBytes < len )
        {
            int sentBytes = 0;

            if ( protocol == Protocol::TCP && isClient() )
            {
                LOG_SOCKET ( TO_C_STR ( "send ( [ %u bytes ] ); from", len ), this );
                sentBytes = ::send ( fd, buffer, len, 0 );
            }
            else
            {
                if ( this->address.empty() && address.empty() )
                {
                    LOG_SOCKET ( "No address: cannot sendto over", this );
                    return false;
                }

                const IpAddrPort& addr = ( address.empty() ? this->address : address );
                addrinfo *res = addr.updateAddrInfo().get();

                LOG_SOCKET ( TO_C_STR (
                                 ( "sendto ( [ %u bytes ], '" + addr.str() + "' ); from" ).c_str(), len ), this );
                sentBytes = ::sendto ( fd, buffer, len, 0, res->ai_addr, res->ai_addrlen );
            }

            if ( sentBytes == SOCKET_ERROR )
            {
                LOG_SOCKET ( ( ( ( protocol == Protocol::TCP && isClient() ) ? "send failed: " : "sendto failed: " )
                               + getLastWinSockError() ).c_str(), this );
                return false;
            }

            totalBytes += sentBytes;
        }

        return true;
    }
    else
    {
        LOG ( "Cannot send to '%s' over unconnected %s socket!", address.c_str(), TO_C_STR ( protocol ) );
        return false;
    }
}

bool Socket::recv ( char *buffer, size_t& len )
{
    assert ( isClient() == true );

    int recvBytes = ::recv ( fd, buffer, len, 0 );

    if ( recvBytes == SOCKET_ERROR )
    {
        LOG_SOCKET ( ( "recvfrom failed: " + getLastWinSockError() ).c_str(), this );
        return false;
    }

    len = recvBytes;
    return true;
}

bool Socket::recv ( char *buffer, size_t& len, IpAddrPort& address )
{
    assert ( protocol == Protocol::UDP );

    sockaddr_storage sa;
    int saLen = sizeof ( sa );

    int recvBytes = ::recvfrom ( fd, buffer, len, 0, ( struct sockaddr * ) &sa, &saLen );

    if ( recvBytes == SOCKET_ERROR )
    {
        LOG_SOCKET ( ( "recvfrom failed: " + getLastWinSockError() ).c_str(), this );
        return false;
    }

    // TODO IPv6
    char addr[INET6_ADDRSTRLEN];
    inet_ntop ( sa.ss_family, & ( ( ( struct sockaddr_in * ) &sa )->sin_addr ), addr, sizeof ( addr ) );

    address.addr = addr;
    address.port = ntohs ( ( ( struct sockaddr_in * ) &sa )->sin_port );

    len = recvBytes;
    return true;
}

void Socket::setPacketLoss ( uint8_t percentage ) { packetLoss = percentage; }

ostream& operator<< ( ostream& os, Socket::Protocol protocol )
{
    switch ( protocol )
    {
        case Socket::Protocol::TCP:
            return ( os << "TCP" );

        case Socket::Protocol::UDP:
            return ( os << "UDP" );
    }

    return ( os << "Unknown socket protocol!" );
}

ostream& operator<< ( ostream& os, Socket::State state )
{
    switch ( state )
    {
        case Socket::State::Listening:
            return ( os << "Listening" );

        case Socket::State::Connecting:
            return ( os << "Connecting" );

        case Socket::State::Connected:
            return ( os << "Connected" );

        case Socket::State::Disconnected:
            return ( os << "Disconnected" );
    }

    return ( os << "Unknown socket state!" );
}

const char *inet_ntop ( int af, const void *src, char *dst, size_t size )
{
    if ( af == AF_INET )
    {
        sockaddr_in in;
        memset ( &in, 0, sizeof ( in ) );
        in.sin_family = AF_INET;
        memcpy ( &in.sin_addr, src, sizeof ( in_addr ) );
        getnameinfo ( ( sockaddr * ) &in, sizeof ( sockaddr_in ), dst, size, 0, 0, NI_NUMERICHOST );
        return dst;
    }
    else if ( af == AF_INET6 )
    {
        sockaddr_in6 in;
        memset ( &in, 0, sizeof ( in ) );
        in.sin6_family = AF_INET6;
        memcpy ( &in.sin6_addr, src, sizeof ( in_addr6 ) );
        getnameinfo ( ( sockaddr * ) &in, sizeof ( sockaddr_in6 ), dst, size, 0, 0, NI_NUMERICHOST );
        return dst;
    }

    return 0;
}
