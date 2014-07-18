#include "SocketManager.h"
#include "Socket.h"
#include "Logger.h"
#include "Utilities.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>
#include <unordered_set>
#include <algorithm>

using namespace std;


#define READ_BUFFER_SIZE ( 1024 * 4096 )

#define SET_BLOCKING_MODE(VALUE)                                            \
    do {                                                                    \
        u_long flag = VALUE;                                                \
        if ( ioctlsocket ( fd, FIONBIO, &flag ) != 0 ) {                    \
            err = WSAGetLastError();                                        \
            LOG_SOCKET ( this, "%s; ioctlsocket failed", err );             \
            closesocket ( fd );                                             \
            fd = 0;                                                         \
            throw err;                                                      \
        }                                                                   \
    } while ( 0 )


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

    WindowsException err;
    shared_ptr<addrinfo> addrInfo;

    // TODO proper binding of IPv6 interfaces
    if ( isClient() && isUDP() )
    {
        // For client UDP sockets, bind to any available local port
        addrInfo = getAddrInfo ( "", 0, true, true );
    }
    else
    {
        // Otherwise bind to the given address and port
        addrInfo = getAddrInfo ( address.addr, address.port, true, isServer() || isUDP() );
    }

    addrinfo *res = addrInfo.get();

    for ( ; res; res = res->ai_next )
    {
        if ( isTCP() )
            fd = ::socket ( res->ai_family, SOCK_STREAM, IPPROTO_TCP );
        else
            fd = ::socket ( res->ai_family, SOCK_DGRAM, IPPROTO_UDP );

        if ( fd == INVALID_SOCKET )
        {
            err = WSAGetLastError();
            LOG_SOCKET ( this, "%s; socket failed", err );
            fd = 0;
            continue;
        }

        if ( isClient() )
        {
            if ( isTCP() )
            {
                SET_BLOCKING_MODE ( 1 );

                if ( ::connect ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
                {
                    int error = WSAGetLastError();

                    // Sucessful non-blocking connect
                    if ( error == WSAEWOULDBLOCK || error == WSAEINVAL )
                    {
                        SET_BLOCKING_MODE ( 0 );
                        break;
                    }

                    err = error;
                    LOG_SOCKET ( this, "%s; connect failed", err );
                    closesocket ( fd );
                    fd = 0;
                    continue;
                }
            }
            else
            {
                if ( ::bind ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
                {
                    err = WSAGetLastError();
                    LOG_SOCKET ( this, "%s; bind failed", err );
                    closesocket ( fd );
                    fd = 0;
                    continue;
                }

                // Successful bind
                break;
            }
        }
        else
        {
            char yes = 1;

            // SO_REUSEADDR can replace existing port binds
            // SO_EXCLUSIVEADDRUSE only replaces if not exact match, so it is safer
            if ( setsockopt ( fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &yes, 1 ) == SOCKET_ERROR )
            {
                err = WSAGetLastError();
                LOG_SOCKET ( this, "%s; setsockopt failed", err );
                closesocket ( fd );
                fd = 0;
                throw err;
            }

            if ( ::bind ( fd, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR )
            {
                err = WSAGetLastError();
                LOG_SOCKET ( this, "%s; bind failed", err );
                closesocket ( fd );
                fd = 0;
                continue;
            }

            if ( isTCP() && ( ::listen ( fd, SOMAXCONN ) == SOCKET_ERROR ) )
            {
                err = WSAGetLastError();
                LOG_SOCKET ( this, "%s; listen failed", err );
                closesocket ( fd );
                fd = 0;
                throw err;
            }

            // Sucessful bind
            break;
        }
    }

    if ( fd == 0 )
    {
        LOG_SOCKET ( this, "%s; init failed", err );
        throw err;
    }

    // Update the address to resolve hostname
    if ( !address.empty() )
    {
        if ( isTCP() && res )
        {
            address.addr = getAddrFromSockAddr ( res->ai_addr );
        }
        else
        {
            addrInfo = getAddrInfo ( address.addr, address.port, true );

            for ( res = addrInfo.get(); res; res = res->ai_next )
            {
                if ( !res )
                    continue;

                address.addr = getAddrFromSockAddr ( res->ai_addr );
                break;
            }
        }
    }

    // Update the local port if bound to any available port
    if ( address.port == 0 )
    {
        sockaddr_storage sas;
        int saLen = sizeof ( sas );

        if ( getsockname ( fd, ( sockaddr * ) &sas, &saLen ) == SOCKET_ERROR )
        {
            err = WSAGetLastError();
            LOG_SOCKET ( this, "%s; getsockname failed", err );
            closesocket ( fd );
            fd = 0;
            throw err;
        }

        address.port = getPortFromSockAddr ( ( sockaddr * ) &sas );
    }
}

MsgPtr Socket::share ( int processId )
{
    shared_ptr<WSAPROTOCOL_INFO> info ( new WSAPROTOCOL_INFO() );

    if ( WSADuplicateSocket ( fd, processId, info.get() ) )
    {
        LOG_SOCKET ( this, "%s; WSADuplicateSocket failed", WindowsException ( WSAGetLastError() ) );
        return 0;
    }

    // Workaround for Wine, because apparently these aren't set
    if ( isTCP() )
    {
        info->iSocketType = SOCK_STREAM;
        info->iProtocol = IPPROTO_TCP;
    }
    else
    {
        info->iSocketType = SOCK_DGRAM;
        info->iProtocol = IPPROTO_UDP;
    }

    SocketManager::get().remove ( this );
    return MsgPtr ( new SocketShareData ( address, protocol, state, readBuffer, readPos, info ) );
}

bool Socket::send ( const char *buffer, size_t len )
{
    assert ( isClient() == true );
    assert ( fd != 0 );

    size_t totalBytes = 0;

    while ( totalBytes < len )
    {
        int sentBytes = SOCKET_ERROR;

        if ( isTCP() )
        {
            LOG_SOCKET ( this, "send ( [ %u bytes ] )", len );
            sentBytes = ::send ( fd, buffer, len, 0 );
        }
        else
        {
            LOG_SOCKET ( this, "sendto ( [ %u bytes ], '%s' )", len, address );
            sentBytes = ::sendto ( fd, buffer, len, 0,
                                   address.getAddrInfo()->ai_addr, address.getAddrInfo()->ai_addrlen );
        }

        if ( sentBytes == SOCKET_ERROR )
        {
            WindowsException err = WSAGetLastError();
            if ( isTCP() )
                LOG_SOCKET ( this, "%s; send failed", err );
            else
                LOG_SOCKET ( this, "%s; sendto failed", err );
            disconnect();
            return false;
        }

        totalBytes += sentBytes;
    }

    return true;
}

bool Socket::send ( const char *buffer, size_t len, const IpAddrPort& address )
{
    if ( fd == 0 || isDisconnected() )
    {
        LOG_SOCKET ( this, "Cannot send over disconnected socket" );
        return false;
    }

    assert ( isUDP() == true );
    assert ( fd != 0 );

    size_t totalBytes = 0;

    while ( totalBytes < len )
    {
        LOG_SOCKET ( this, "sendto ( [ %u bytes ], '%s' )", len, address );
        int sentBytes = ::sendto ( fd, buffer, len, 0,
                                   address.getAddrInfo()->ai_addr, address.getAddrInfo()->ai_addrlen );

        if ( sentBytes == SOCKET_ERROR )
        {
            LOG_SOCKET ( this, "%s; sendto failed", WindowsException ( WSAGetLastError() ) );
            return false;
        }

        totalBytes += sentBytes;
    }

    return true;
}

bool Socket::recv ( char *buffer, size_t& len )
{
    assert ( isClient() == true );
    assert ( fd != 0 );

    int recvBytes = ::recv ( fd, buffer, len, 0 );

    if ( recvBytes == SOCKET_ERROR )
    {
        LOG_SOCKET ( this, "%s; recvfrom failed", WindowsException ( WSAGetLastError() ) );
        return false;
    }

    len = recvBytes;
    return true;
}

bool Socket::recv ( char *buffer, size_t& len, IpAddrPort& address )
{
    assert ( isUDP() == true );
    assert ( fd != 0 );

    sockaddr_storage sas;
    int saLen = sizeof ( sas );

    int recvBytes = ::recvfrom ( fd, buffer, len, 0, ( sockaddr * ) &sas, &saLen );

    if ( recvBytes == SOCKET_ERROR )
    {
        LOG_SOCKET ( this, "%s; recvfrom failed", WindowsException ( WSAGetLastError() ) );
        return false;
    }

    len = recvBytes;
    address = ( sockaddr * ) &sas;
    return true;
}

void Socket::readEvent()
{
    char *bufferEnd = & ( readBuffer[readPos] );
    size_t bufferLen = readBuffer.size() - readPos;

    IpAddrPort address = getRemoteAddress();
    bool success = false;

    if ( isTCP() )
        success = recv ( bufferEnd, bufferLen );
    else
        success = recv ( bufferEnd, bufferLen, address );

    if ( !success )
    {
        // Disconnect the socket if an error occured during read
        LOG_SOCKET ( this, "disconnect due to read error" );
        if ( isTCP() )
            disconnectEvent();
        else
            disconnect();
        return;
    }

    // Simulated packet loss
    if ( rand() % 100 < packetLoss )
    {
        LOG ( "Discarding [ %u bytes ] from '%s'", bufferLen, address );
        return;
    }

    // Increment the buffer position
    readPos += bufferLen;
    LOG ( "Read [ %u bytes ] from '%s'", bufferLen, address );

    // Handle zero byte packets
    if ( bufferLen == 0 )
    {
        LOG ( "Decoded [ 0 bytes ] to 'NullMsg'" );
        readEvent ( NullMsg, address );
        return;
    }

    LOG ( "Base64 : %s", toBase64 ( bufferEnd, min ( 256u, bufferLen ) ) );

    // Try to decode as many messages from the buffer as possible
    for ( ;; )
    {
        size_t consumed = 0;
        MsgPtr msg = Serializable::decode ( &readBuffer[0], readPos, consumed );

        // Abort if a message could not be decoded
        if ( !msg.get() )
            return;

        LOG ( "Decoded [ %u bytes ] to '%s'", consumed, msg );
        readEvent ( msg, address );

        // Abort if the socket is de-allocated
        if ( !SocketManager::get().isAllocated ( this ) )
            return;

        // Abort if socket is disconnected
        if ( isDisconnected() )
            return;

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

void SocketShareData::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( address, protocol, state, readBuffer, readPos,
         info->dwServiceFlags1,
         info->dwServiceFlags2,
         info->dwServiceFlags3,
         info->dwServiceFlags4,
         info->dwProviderFlags,
         info->ProviderId.Data1,
         info->ProviderId.Data2,
         info->ProviderId.Data3,
         info->ProviderId.Data4,
         info->dwCatalogEntryId,
         info->ProtocolChain.ChainLen,
         info->ProtocolChain.ChainEntries,
         info->iVersion,
         info->iAddressFamily,
         info->iMaxSockAddr,
         info->iMinSockAddr,
         info->iSocketType,
         info->iProtocol,
         info->iProtocolMaxOffset,
         info->iNetworkByteOrder,
         info->iSecurityScheme,
         info->dwMessageSize,
         info->dwProviderReserved,
         info->szProtocol );
}

void SocketShareData::load ( cereal::BinaryInputArchive& ar )
{
    info.reset ( new WSAPROTOCOL_INFO() );

    ar ( address, protocol, state, readBuffer, readPos,
         info->dwServiceFlags1,
         info->dwServiceFlags2,
         info->dwServiceFlags3,
         info->dwServiceFlags4,
         info->dwProviderFlags,
         info->ProviderId.Data1,
         info->ProviderId.Data2,
         info->ProviderId.Data3,
         info->ProviderId.Data4,
         info->dwCatalogEntryId,
         info->ProtocolChain.ChainLen,
         info->ProtocolChain.ChainEntries,
         info->iVersion,
         info->iAddressFamily,
         info->iMaxSockAddr,
         info->iMinSockAddr,
         info->iSocketType,
         info->iProtocol,
         info->iProtocolMaxOffset,
         info->iNetworkByteOrder,
         info->iSecurityScheme,
         info->dwMessageSize,
         info->dwProviderReserved,
         info->szProtocol );
}
