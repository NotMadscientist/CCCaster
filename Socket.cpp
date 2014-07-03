#include "Socket.h"
#include "Log.h"
#include "Util.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>

#define READ_BUFFER_SIZE ( 1024 * 4096 )

using namespace std;

Socket::Socket ( const IpAddrPort& address, Protocol protocol )
    : address ( address ), protocol ( protocol ), owner ( 0 ), state ( State::Disconnected ), fd ( 0 )
    , readBuffer ( READ_BUFFER_SIZE, ( char ) 0 ), readPos ( 0 ), packetLoss ( 0 )
{
}

Socket::~Socket()
{
    disconnect();
}

void Socket::disconnect()
{
    if ( fd )
        closesocket ( fd );

    fd = readPos = packetLoss = 0;
    state = State::Disconnected;
    owner = 0;
    readBuffer.clear();
}

void Socket::init()
{
    assert ( fd == 0 );

    shared_ptr<addrinfo> addrInfo;

    // TODO proper binding of IPv6 interfaces
    if ( isClient() && protocol == Protocol::UDP )
    {
        // For client UDP sockets, bind to any available local port
        addrInfo = getAddrInfo ( "", 0, true, true );
    }
    else
    {
        // Otherwise bind to the given address and port
        addrInfo = getAddrInfo ( address.addr, address.port, true, isServer() || protocol == Protocol::UDP );
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

        u_long yes = 1;

        // FIONBIO sets non-blocking socket operation
        if ( ioctlsocket ( fd, FIONBIO, &yes ) != 0 )
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

            // SO_REUSEADDR can replace existing port binds
            // SO_EXCLUSIVEADDRUSE only replaces if not exact match, so it is safer
            if ( setsockopt ( fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &yes, 1 ) == SOCKET_ERROR )
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

    if ( fd == 0 )
    {
        LOG_SOCKET ( "init failed", this );
        throw "something"; // TODO
    }

    // Update the local port if bound to any available port
    if ( address.port == 0 )
    {
        sockaddr_storage sa;
        int saLen = sizeof ( sa );

        if ( getsockname ( fd, ( struct sockaddr * ) &sa, &saLen ) == SOCKET_ERROR )
        {
            LOG_SOCKET ( ( "getsockname failed: " + getLastWinSockError() ).c_str(), this );
            closesocket ( fd );
            fd = 0;
            throw "something"; // TODO
        }

        address.port = getPortFromSockAddr ( sa );
    }
}

bool Socket::send ( const char *buffer, size_t len )
{
    assert ( isClient() == true );
    assert ( isConnected() == true );
    assert ( fd != 0 );

    size_t totalBytes = 0;

    while ( totalBytes < len )
    {
        int sentBytes = SOCKET_ERROR;

        if ( protocol == Protocol::TCP )
        {
            LOG_SOCKET ( TO_C_STR ( "send ( [ %u bytes ] ); from", len ), this );
            sentBytes = ::send ( fd, buffer, len, 0 );
        }
        else
        {
            LOG_SOCKET ( TO_C_STR ( ( "sendto ( [ %u bytes ], '" + address.str() + "' ); from" ).c_str(), len ), this );
            sentBytes = ::sendto ( fd, buffer, len, 0, address.addrInfo->ai_addr, address.addrInfo->ai_addrlen );
        }

        if ( sentBytes == SOCKET_ERROR )
        {
            LOG_SOCKET ( ( ( protocol == Protocol::TCP ? "send failed: " : "sendto failed: " )
                           + getLastWinSockError() ).c_str(), this );
            disconnect();
            return false;
        }

        totalBytes += sentBytes;
    }

    return true;
}

bool Socket::send ( const char *buffer, size_t len, const IpAddrPort& address )
{
    assert ( protocol == Protocol::UDP );
    assert ( fd != 0 );

    size_t totalBytes = 0;

    while ( totalBytes < len )
    {
        LOG_SOCKET ( TO_C_STR ( ( "sendto ( [ %u bytes ], '" + address.str() + "' ); from" ).c_str(), len ), this );
        int sentBytes = ::sendto ( fd, buffer, len, 0, address.addrInfo->ai_addr, address.addrInfo->ai_addrlen );

        if ( sentBytes == SOCKET_ERROR )
        {
            LOG_SOCKET ( ( "sendto failed: " + getLastWinSockError() ).c_str(), this );
            return false;
        }

        totalBytes += sentBytes;
    }

    return true;
}

bool Socket::recv ( char *buffer, size_t& len )
{
    assert ( isClient() == true );
    assert ( isConnected() == true );
    assert ( fd != 0 );

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
    assert ( fd != 0 );

    sockaddr_storage sa;
    int saLen = sizeof ( sa );

    int recvBytes = ::recvfrom ( fd, buffer, len, 0, ( struct sockaddr * ) &sa, &saLen );

    if ( recvBytes == SOCKET_ERROR )
    {
        LOG_SOCKET ( ( "recvfrom failed: " + getLastWinSockError() ).c_str(), this );
        return false;
    }

    len = recvBytes;
    address = sa;
    return true;
}

void Socket::readEvent()
{
    LOG_SOCKET ( "Read from", this );

    char *bufferEnd = & ( readBuffer[readPos] );
    size_t bufferLen = readBuffer.size() - readPos;

    IpAddrPort address = getRemoteAddress();
    bool success = false;

    if ( protocol == Protocol::TCP )
        success = recv ( bufferEnd, bufferLen );
    else
        success = recv ( bufferEnd, bufferLen, address );

    if ( !success )
    {
        // Disconnect the socket if an error occured during read
        LOG_SOCKET ( "Disconnected", this );
        if ( protocol == Protocol::TCP )
            disconnectEvent();
        else
            disconnect();
        return;
    }

    // Simulated packet loss
    if ( rand() % 100 < packetLoss )
    {
        LOG ( "Discarding [ %u bytes ] from '%s'", bufferLen, address.c_str() );
        return;
    }

    // Increment the buffer position
    readPos += bufferLen;
    LOG ( "Read [ %u bytes ] from '%s'", bufferLen, address.c_str() );

    // Handle zero byte packets
    if ( bufferLen == 0 )
    {
        LOG ( "Decoded [ 0 bytes ] to 'NullMsg'" );
        readEvent ( NullMsg, address );
        return;
    }

    LOG ( "Base64 : %s", toBase64 ( bufferEnd, bufferLen ).c_str() );

    // Try to decode as many messages from the buffer as possible
    for ( ;; )
    {
        size_t consumed = 0;
        MsgPtr msg = Serializable::decode ( &readBuffer[0], readPos, consumed );

        if ( !msg.get() )
            break;

        LOG ( "Decoded [ %u bytes ] to '%s'", consumed, TO_C_STR ( msg ) );
        readEvent ( msg, address );

        // TODO handle this case
//         // Abort if the socket is no longer alive
//         if ( allocatedSockets.find ( socket ) == allocatedSockets.end() )
//             break;

        assert ( consumed <= readPos );

        // Erase the consumed bytes (shifting the array)
        readBuffer.erase ( 0, consumed );
        readPos -= consumed;
    }
}

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
