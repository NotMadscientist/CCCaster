#include "SmartSocket.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Logger.h"

#include <ws2tcpip.h>

using namespace std;

// TODO add more state to this log macro
#define LOG_SMART_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, FORMAT, ## __VA_ARGS__)

#define SEND_INTERVAL ( 50 )

const IpAddrPort vpsAddress = "198.12.67.179:3939"; // vps.mi.ku.lc
// const IpAddrPort vpsAddress = "23.95.23.238:3939"; // hatsune.mi.ku.lc


struct Match
{
    static uint32_t decode ( const char *buffer, size_t len, size_t& consumed )
    {
        static const string header = "match";

        if ( len < header.size() + sizeof ( uint32_t ) || string ( buffer, header.size() ) != header )
        {
            consumed = 0;
            return 0;
        }

        consumed = header.size() + sizeof ( uint32_t );
        return * ( uint32_t * ) ( buffer + header.size() );
    }
};


struct UdpInfo
{
    uint32_t matchId = 0;

    IpAddrPort address;

    UdpInfo() {}
    UdpInfo ( uint32_t matchId, const string& address ) : matchId ( matchId ), address ( address ) {}

    static UdpInfo decode ( const char *buffer, size_t len, size_t& consumed )
    {
        static const string header = "info";

        if ( len < header.size() + sizeof ( uint32_t ) || string ( buffer, header.size() ) != header )
        {
            consumed = 0;
            return UdpInfo();
        }

        const size_t start = header.size() + sizeof ( uint32_t );

        size_t i, end = 0;

        for ( i = 0; i < 22; ++i ) // max string length ("255.255.255.255:65535\0")
        {
            if ( start + i >= len )
                break;

            if ( buffer[start + i] == '\0' )
            {
                end = start + i;
                break;
            }
        }

        // Not enough data or failed to find null-terminator
        if ( end == 0 || i == 22 )
        {
            consumed = 0;
            return UdpInfo();
        }

        consumed = end + 1;
        return UdpInfo ( * ( uint32_t * ) &buffer[header.size()], string ( buffer + start, end - start ) );
    }
};


/* Tunnel protocol

    1 - Host opens a TCP socket to the server and sends its hosting port (uint16_t).
        Host should maintain the socket connection; reconnect and resend if needed.

    2 - Client opens a TCP socket to the server and sends an address string ("<ip>:<port>").

    2 - Server tries to matchmake:
        If a matching host if found, the server sends matchId to host AND client to signal match.
        Otherwise disconnects the client if no matching host exists.

    3 - On match, host and client both create a new UDP socket (bind any port),
        and start sending match info to the server's UDP port.

    4 - Server recvs UDP packets, and relays UDP address info to each side over TCP

    5 - Host and client can now connect over the UDP tunnel

*/

SmartSocket::SmartSocket ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::Smart )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    this->state = State::Listening;

    vpsSocket = TcpSocket::connect ( this, vpsAddress, true ); // Raw socket

    tunSocket = UdpSocket::bind ( this, 0, true ); // Raw socket

    // // Listen for direct connections in parallel
    // if ( protocol == Protocol::TCP )
    //     directSocket = TcpSocket::listen ( this, port );
    // else
    //     directSocket = UdpSocket::listen ( this, port );
}

SmartSocket::SmartSocket ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol )
    : Socket ( owner, address, Protocol::Smart )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    this->state = State::Connecting;

#if 0
    // Try to connect normally first
    if ( protocol == Protocol::TCP )
        directSocket = TcpSocket::connect ( this, address );
    else
        directSocket = UdpSocket::connect ( this, address );
#else
    vpsSocket = TcpSocket::connect ( this, vpsAddress, true );
#endif
}

SmartSocket::~SmartSocket()
{
    disconnect();
}

void SmartSocket::disconnect()
{
    Socket::disconnect();

    matchId = 0;
    tunAddress.clear();

    directSocket.reset();
    vpsSocket.reset();
    tunSocket.reset();

    sendTimer.reset();
    connectTimer.reset();
}

void SmartSocket::acceptEvent ( Socket *serverSocket )
{
    ASSERT ( serverSocket == directSocket.get() || serverSocket == tunSocket.get() );

    if ( owner )
        owner->acceptEvent ( this );
}

void SmartSocket::connectEvent ( Socket *socket )
{
    if ( socket == directSocket.get() || socket == tunSocket.get() )
    {
        sendTimer.reset();
        connectTimer.reset();

        this->state = State::Connected;

        if ( owner )
            owner->connectEvent ( this );
    }
    else if ( socket == vpsSocket.get() )
    {
        if ( isServer() )
        {
            vpsSocket->send ( ( char * ) &address.port, sizeof ( uint16_t ) );
        }
        else
        {
            string buffer = address.str();
            vpsSocket->send ( &buffer[0], buffer.size() );

            connectTimer.reset ( new Timer ( this ) );
            connectTimer->start ( connectTimeout );
        }

        // Wait for callback to gotMatch
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::disconnectEvent ( Socket *socket )
{
    if ( socket == directSocket.get() )
    {
        LOG_SMART_SOCKET ( this, "Switch to UDP tunnel" );

        directSocket.reset();

        vpsSocket = TcpSocket::connect ( this, vpsAddress, true );

        if ( owner )
            owner->switchedToUdpTunnel ( this );
    }
    else if ( socket == tunSocket.get() )
    {
        LOG_SMART_SOCKET ( this, "Tunnel socket disconnected" );

        Socket::Owner *owner = this->owner;

        disconnect();

        if ( owner )
            owner->disconnectEvent ( this );
    }
    else if ( socket == vpsSocket.get() )
    {
        // TODO reconnect but throttle attempts over a time period in case the VPS is down
        // vpsSocket = TcpSocket::connect ( this, vpsAddress, true ); // Raw socket

        LOG_SMART_SOCKET ( this, "vpsSocket disconnected" );

        Socket::Owner *owner = this->owner;

        disconnect();

        if ( owner )
            owner->disconnectEvent ( this );
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->readEvent ( this, msg, address );
}

void SmartSocket::readEvent ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address )
{
    if ( socket == tunSocket.get() )
    {
        LOG ( "Read [ %u bytes ] from '%s' (tunSocket)", len, address );
        return;
    }

    ASSERT ( socket == vpsSocket.get() );

    vpsSocket->readPos += len;
    LOG ( "Read [ %u bytes ] from '%s'; %u bytes remaining in buffer", len, address, vpsSocket->readPos );

    if ( len > 0 && len <= 256 )
        LOG ( "Base64 : %s", toBase64 ( buffer, len ) );

    uint32_t id;
    UdpInfo info;
    size_t consumed;

    for ( ;; )
    {
        if ( ( id = Match::decode ( &vpsSocket->readBuffer[0], vpsSocket->readPos, consumed ) ) )
        {
            LOG_SMART_SOCKET ( this, "gotMatch ( %u )", id );

            vpsSocket->consumeBuffer ( consumed );

            gotMatch ( id );
        }
        else if ( ( info = UdpInfo::decode ( &vpsSocket->readBuffer[0], vpsSocket->readPos, consumed ) ).matchId )
        {
            LOG_SMART_SOCKET ( this, "gotUdpInfo ( %u, '%s' )", info.matchId, info.address );

            vpsSocket->consumeBuffer ( consumed );

            gotUdpInfo ( info.matchId, info.address );
        }
        else
        {
            // No more data to parse
            break;
        }
    }
}

void SmartSocket::timerExpired ( Timer *timer )
{
    if ( timer == connectTimer.get() )
    {
        LOG_SMART_SOCKET ( this, "No matching host found" );

        Socket::Owner *owner = this->owner;

        disconnect();

        if ( owner )
            owner->disconnectEvent ( this );
    }
    else if ( timer == sendTimer.get() )
    {
        if ( isServer() )
        {
            for ( const auto& kv : pendingClients )
            {
                const TunnelClient& tunClient = kv.second;

                if ( tunClient.address.empty() )
                {
                    ASSERT ( tunClient.matchId != 0 );

                    char buffer[5];
                    buffer[0] = ( isServer() ? 0 : 1 );
                    ( * ( uint32_t * ) &buffer[1] ) = tunClient.matchId;

                    tunSocket->send ( buffer, sizeof ( buffer ), vpsAddress );
                }
                else
                {
                    tunSocket->send ( NullMsg, tunClient.address );
                }
            }
        }
        else
        {
            if ( tunAddress.empty() )
            {
                ASSERT ( matchId != 0 );

                char buffer[5];
                buffer[0] = ( isServer() ? 0 : 1 );
                ( * ( uint32_t * ) &buffer[1] ) = matchId;

                tunSocket->send ( buffer, sizeof ( buffer ), vpsAddress );
            }
            else
            {
                tunSocket->send ( NullMsg, tunAddress );
            }
        }

        sendTimer->start ( SEND_INTERVAL );
    }
    else if ( pendingTimers.find ( timer ) != pendingTimers.end() )
    {
        ASSERT ( isServer() == true );

        auto it = pendingClients.find ( pendingTimers[timer] );

        LOG_SMART_SOCKET ( this, "matchId=%u; address='%s'; Client timed out", it->first, it->second.address );

        pendingClients.erase ( it );
        pendingTimers.erase ( timer );

        if ( pendingClients.empty() && pendingTimers.empty() )
            sendTimer.reset();
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::gotMatch ( uint32_t matchId )
{
    ASSERT ( matchId != 0 );

    if ( isServer() )
    {
        TunnelClient tunClient;
        tunClient.matchId = matchId;
        tunClient.timer.reset ( new Timer ( this ) );
        tunClient.timer->start ( connectTimeout );

        pendingClients[matchId] = tunClient;
        pendingTimers[tunClient.timer.get()] = matchId;
    }
    else
    {
        this->matchId = matchId;

        tunSocket = UdpSocket::bind ( this, vpsAddress, true ); // Raw socket
    }

    if ( sendTimer )
        return;

    sendTimer.reset ( new Timer ( this ) );
    sendTimer->start ( SEND_INTERVAL );
}

void SmartSocket::gotUdpInfo ( uint32_t matchId, const IpAddrPort& address )
{
    ASSERT ( matchId != 0 );
    ASSERT ( address.empty() == false );

    if ( isServer() )
    {
        for ( auto& kv : pendingClients )
        {
            TunnelClient& tunClient = kv.second;

            if ( tunClient.matchId == matchId )
            {
                tunClient.address = address;
                tunClient.timer->start ( connectTimeout );
                break;
            }
        }
    }
    else
    {
        connectTimer.reset();

        this->tunAddress = address;
    }
}

SocketPtr SmartSocket::listen ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol )
{
    return SocketPtr ( new SmartSocket ( owner, port, protocol ) );
}

SocketPtr SmartSocket::connect ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol )
{
    string addr = getAddrFromSockAddr ( address.getAddrInfo()->ai_addr ); // Resolve IP address first
    return SocketPtr ( new SmartSocket ( owner, { addr, address.port }, protocol ) );
}

SocketPtr SmartSocket::accept ( Socket::Owner *owner )
{
    if ( directSocket )
        return directSocket->accept ( owner );

    if ( tunSocket )
        return tunSocket->accept ( owner );

    return 0;
}

#define BOILERPLATE_SEND(...)                                                           \
    do {                                                                                \
        if ( !isConnected() )                                                           \
            return false;                                                               \
        if ( directSocket )                                                             \
            return directSocket->send ( __VA_ARGS__ );                                  \
        if ( tunSocket )                                                                \
            return tunSocket->send ( __VA_ARGS__ );                                     \
        return false;                                                                   \
    } while ( 0 )

bool SmartSocket::send ( const char *buffer, size_t len )
{
    BOILERPLATE_SEND ( buffer, len );
}

bool SmartSocket::send ( const char *buffer, size_t len, const IpAddrPort& address )
{
    BOILERPLATE_SEND ( buffer, len, address );
}

bool SmartSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    BOILERPLATE_SEND ( message, address );
}

bool SmartSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    BOILERPLATE_SEND ( message, address );
}

bool SmartSocket::send ( const MsgPtr& message, const IpAddrPort& address )
{
    BOILERPLATE_SEND ( message, address );
}
