#include "SmartSocket.hpp"
#include "TcpSocket.hpp"
#include "UdpSocket.hpp"
#include "Logger.hpp"

#include <ws2tcpip.h>

using namespace std;

// TODO add more state to this log macro
#define LOG_SMART_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, FORMAT, ## __VA_ARGS__)

#define SEND_INTERVAL ( 50 )

static const vector<IpAddrPort> relayServers =
{
    "104.206.199.123:3939",
    "192.210.227.23:3939",
};

/* Tunnel protocol

    1 - Host opens a TCP socket to the server and sends its TypedHostingPort.
        Host should maintain the socket connection; reconnect and resend if needed.

    2 - Client opens a TCP socket to the server and sends its TypedConnectionAddress.

    2 - Server tries to match-make:
        If a matching host if found, the server sends MatchInfo to host AND client over TCP.
        Otherwise disconnects the client if no matching host exists.

    3 - On match, host and client both create a new UDP socket bound to any port,
        and start repeatedly sending UdpData to the server's UDP port.

    4 - Server recvs UdpData from the client and sends TunInfo ONCE to the host over TCP.
        Server recvs UdpData from the host and sends TunInfo ONCE to the client over TCP.

    5 - Host and client can now connect over the address specified in TunInfo.

  Binary formats (little-endian):

    TypedHostingPort is a char followed by a single uint16_t. The char must by 'T' for TCP or 'U' for UDP.

    TypedConnectionAddress is a NON-null-terminated string, eg. "T<ip>:<port>". The first char is the socket type.

    MatchInfo is "MatchInfo" followed by the matchId.

    UdpData is a uint8_t followed by the matchId. The uint8_t is a boolean flag indicating isClient.

    TunInfo is "TunInfo" followed by the matchId, followed by a NULL-terminated address string (for easier parsing).

    The matchId is always a uint32_t, and should be non-zero.

*/

struct MatchInfo
{
    static uint32_t decode ( const char *buffer, size_t len, size_t& consumed )
    {
        static const string header = "MatchInfo";

        if ( len < header.size() + sizeof ( uint32_t ) || string ( buffer, header.size() ) != header )
        {
            consumed = 0;
            return 0;
        }

        consumed = header.size() + sizeof ( uint32_t );
        return * ( uint32_t * ) ( buffer + header.size() );
    }
};

struct UdpData
{
    char buffer[5];

    UdpData ( bool isClient, uint32_t matchId )
    {
        ASSERT ( matchId != 0 );

        buffer[0] = ( char ) ( isClient ? 1 : 0 );
        memcpy ( &buffer[1], ( char * ) &matchId, sizeof ( uint32_t ) );
    }
};

struct TunInfo
{
    uint32_t matchId = 0;

    IpAddrPort address;

    TunInfo() {}
    TunInfo ( uint32_t matchId, const string& address ) : matchId ( matchId ), address ( address ) {}

    static TunInfo decode ( const char *buffer, size_t len, size_t& consumed )
    {
        static const string header = "TunInfo";

        if ( len < header.size() + sizeof ( uint32_t ) || string ( buffer, header.size() ) != header )
        {
            consumed = 0;
            return TunInfo();
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
            return TunInfo();
        }

        consumed = end + 1;
        return TunInfo ( * ( uint32_t * ) &buffer[header.size()], string ( buffer + start, end - start ) );
    }
};

SmartSocket::SmartSocket ( Owner *owner, uint16_t port, Socket::Protocol protocol )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::Smart, false )
    , _isDirectTCP ( protocol == Protocol::TCP )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    _state = State::Listening;

    _vpsAddress = relayServers.cbegin();
    _vpsSocket = TcpSocket::connect ( this, *_vpsAddress, true ); // Raw socket

    try
    {
        // Listen for direct connections at the same time
        if ( _isDirectTCP )
            _directSocket = TcpSocket::listen ( this, port );
        else
            _directSocket = UdpSocket::listen ( this, port );

        // Update address port
        address.port = _directSocket->address.port;
        address.invalidate();
    }
    catch ( ... )
    {
        LOG ( "Failed to bind directSocket to port %u", port );
    }

    _tunSocket = UdpSocket::listen ( this, 0 );
}

SmartSocket::SmartSocket ( Owner *owner, const IpAddrPort& address, Socket::Protocol protocol, bool forceTun )
    : Socket ( owner, address, Protocol::Smart, false )
    , _isDirectTCP ( protocol == Protocol::TCP )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    _state = State::Connecting;

    if ( forceTun )
    {
        _vpsAddress = relayServers.cbegin();
        _vpsSocket = TcpSocket::connect ( this, *_vpsAddress, true );
        return;
    }

    // Try to connect directly first
    if ( _isDirectTCP )
        _directSocket = TcpSocket::connect ( this, address );
    else
        _directSocket = UdpSocket::connect ( this, address );
}

SmartSocket::~SmartSocket()
{
    disconnect();
}

void SmartSocket::disconnect()
{
    Socket::disconnect();

    _matchId = 0;
    _tunAddress.clear();

    _directSocket.reset();
    _vpsSocket.reset();
    _tunSocket.reset();

    _sendTimer.reset();
    _connectTimer.reset();
}

void SmartSocket::socketAccepted ( Socket *serverSocket )
{
    ASSERT ( serverSocket == _directSocket.get() || serverSocket == _tunSocket.get() );

    _isDirectAccept = ( serverSocket == _directSocket.get() );

    if ( owner )
        owner->socketAccepted ( this );
}

void SmartSocket::socketConnected ( Socket *socket )
{
    if ( socket == _directSocket.get() || socket == _tunSocket.get() )
    {
        _sendTimer.reset();
        _connectTimer.reset();

        _state = State::Connected;

        if ( owner )
            owner->socketConnected ( this );
    }
    else if ( socket == _vpsSocket.get() )
    {
        if ( isServer() )
        {
            char buffer[3];
            buffer[0] = ( _isDirectTCP ? 'T' : 'U' );
            memcpy ( &buffer[1], ( char * ) &address.port, sizeof ( uint16_t ) );

            _vpsSocket->send ( buffer, sizeof ( buffer ) );
        }
        else
        {
            const string buffer = ( _isDirectTCP ? "T" : "U" ) + address.str();

            _vpsSocket->send ( &buffer[0], buffer.size() );

            _connectTimer.reset ( new Timer ( this ) );
            _connectTimer->start ( _connectTimeout );
        }

        // Wait for callback to gotMatch
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::socketDisconnected ( Socket *socket )
{
    if ( socket == _directSocket.get() && isServer() )
    {
        _directSocket.reset();
    }
    else if ( socket == _directSocket.get() && isConnecting() )
    {
        LOG_SMART_SOCKET ( this, "Switching to UDP tunnel" );

        _directSocket.reset();

        _vpsAddress = relayServers.cbegin();
        _vpsSocket = TcpSocket::connect ( this, *_vpsAddress, true ); // Raw socket

        if ( owner )
            ( ( SmartSocket::Owner * ) owner )->smartSocketSwitchedToUDP ( this );
    }
    else if ( ( socket == _directSocket.get() && isConnected() ) || socket == _tunSocket.get() )
    {
        LOG_SMART_SOCKET ( this, "Tunnel socket disconnected" );

        Socket::Owner *const owner = this->owner;

        disconnect();

        if ( owner )
            owner->socketDisconnected ( this );
    }
    else if ( socket == _vpsSocket.get() )
    {
        LOG_SMART_SOCKET ( this, "vpsSocket disconnected" );

        ASSERT ( _vpsAddress != relayServers.cend() );

        ++_vpsAddress;

        if ( _vpsAddress != relayServers.cend() )
        {
            _connectTimer.reset();
            _vpsSocket = TcpSocket::connect ( this, *_vpsAddress, true ); // Raw socket
            return;
        }

        _vpsSocket.reset();

        if ( isConnected() || isServer() )
            return;

        Socket::Owner *const owner = this->owner;

        disconnect();

        if ( owner )
            owner->socketDisconnected ( this );
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->socketRead ( this, msg, address );
}

void SmartSocket::socketRead ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address )
{
    ASSERT ( socket == _vpsSocket.get() );

    _vpsSocket->_readPos += len;
    LOG ( "Read [ %u bytes ] from '%s'; %u bytes remaining in buffer", len, address, _vpsSocket->_readPos );

    if ( len > 0 && len <= 256 )
        LOG ( "Hex: %s", formatAsHex ( buffer, len ) );

    uint32_t id;
    TunInfo tun;
    size_t consumed;

    for ( ;; )
    {
        id = MatchInfo::decode ( &_vpsSocket->_readBuffer[0], _vpsSocket->_readPos, consumed );

        if ( id )
        {
            LOG_SMART_SOCKET ( this, "gotMatch ( %u )", id );

            _vpsSocket->consumeBuffer ( consumed );

            gotMatch ( id );
            continue;
        }

        tun = TunInfo::decode ( &_vpsSocket->_readBuffer[0], _vpsSocket->_readPos, consumed );

        if ( tun.matchId )
        {
            LOG_SMART_SOCKET ( this, "gotTunInfo ( %u, '%s' )", tun.matchId, tun.address );

            _vpsSocket->consumeBuffer ( consumed );

            gotTunInfo ( tun.matchId, tun.address );
            continue;
        }

        // No more data to parse
        break;
    }
}

void SmartSocket::timerExpired ( Timer *timer )
{
    if ( timer == _connectTimer.get() )
    {
        LOG_SMART_SOCKET ( this, "No matching host found" );

        Socket::Owner *const owner = this->owner;

        disconnect();

        if ( owner )
            owner->socketDisconnected ( this );
    }
    else if ( timer == _sendTimer.get() )
    {
        if ( isServer() )
        {
            for ( const auto& kv : _pendingClients )
            {
                const TunnelClient& tunClient = kv.second;
                const UdpData data ( isClient(), tunClient.matchId );

                ASSERT ( _vpsAddress != relayServers.cend() );

                _tunSocket->send ( data.buffer, sizeof ( data.buffer ), *_vpsAddress );

                if ( ! tunClient.address.empty() )
                    _tunSocket->send ( NullMsg, tunClient.address );
            }
        }
        else if ( _matchId )
        {
            const UdpData data ( isClient(), _matchId );

            ASSERT ( _vpsAddress != relayServers.cend() );

            _tunSocket->send ( data.buffer, sizeof ( data.buffer ), *_vpsAddress );

            if ( ! _tunAddress.empty() )
                _tunSocket->send ( NullMsg, _tunAddress );
        }
        else
        {
            _sendTimer.reset();

            LOG ( "No valid matchId; stopped sending UdpData" );
            return;
        }

        _sendTimer->start ( SEND_INTERVAL );
    }
    else if ( _pendingTimers.find ( timer ) != _pendingTimers.end() )
    {
        ASSERT ( isServer() == true );

        const auto it = _pendingClients.find ( _pendingTimers[timer] );

        if ( it != _pendingClients.end() )
        {
            LOG_SMART_SOCKET ( this, "matchId=%u; address='%s'; Client timed out", it->first, it->second.address );

            _pendingClients.erase ( it );
        }

        _pendingTimers.erase ( timer );

        if ( _pendingClients.empty() && _pendingTimers.empty() )
            _sendTimer.reset();
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
        tunClient.timer->start ( _connectTimeout );

        _pendingClients[matchId] = tunClient;
        _pendingTimers[tunClient.timer.get()] = matchId;
    }
    else
    {
        _matchId = matchId;

        ASSERT ( _vpsAddress != relayServers.cend() );

        _tunSocket = UdpSocket::bind ( this, *_vpsAddress );
    }

    if ( _sendTimer )
        return;

    _sendTimer.reset ( new Timer ( this ) );
    _sendTimer->start ( SEND_INTERVAL );
}

void SmartSocket::gotTunInfo ( uint32_t matchId, const IpAddrPort& address )
{
    ASSERT ( matchId != 0 );
    ASSERT ( address.empty() == false );

    if ( isServer() )
    {
        for ( auto& kv : _pendingClients )
        {
            TunnelClient& tunClient = kv.second;

            if ( tunClient.matchId == matchId )
            {
                tunClient.address = address;
                tunClient.timer->start ( _connectTimeout );
                break;
            }
        }
    }
    else
    {
        _connectTimer.reset();

        _tunAddress = address;

        ASSERT ( _tunSocket.get() != 0 );
        ASSERT ( _tunSocket->isUDP() == true );

        _tunSocket->getAsUDP().connect ( address );
    }
}

SocketPtr SmartSocket::listenTCP ( Owner *owner, uint16_t port )
{
    return SocketPtr ( new SmartSocket ( owner, port, Socket::Protocol::TCP ) );
}

SocketPtr SmartSocket::listenUDP ( Owner *owner, uint16_t port )
{
    return SocketPtr ( new SmartSocket ( owner, port, Socket::Protocol::UDP ) );
}

SocketPtr SmartSocket::connectTCP ( Owner *owner, const IpAddrPort& address, bool forceTunnel )
{
    const string addr = getAddrFromSockAddr ( address.getAddrInfo()->ai_addr ); // Resolve IP address first
    return SocketPtr ( new SmartSocket ( owner, { addr, address.port }, Socket::Protocol::TCP, forceTunnel ) );
}

SocketPtr SmartSocket::connectUDP ( Owner *owner, const IpAddrPort& address, bool forceTunnel )
{
    const string addr = getAddrFromSockAddr ( address.getAddrInfo()->ai_addr ); // Resolve IP address first
    return SocketPtr ( new SmartSocket ( owner, { addr, address.port }, Socket::Protocol::UDP, forceTunnel ) );
}

bool SmartSocket::isTunnel() const
{
    return ( isClient() && _tunSocket && !_tunSocket->getAsUDP().isConnectionLess() && _tunSocket->isConnected() );
}

SocketPtr SmartSocket::accept ( Socket::Owner *owner )
{
    if ( _isDirectAccept && _directSocket )
        return _directSocket->accept ( owner );

    SocketPtr socket = _tunSocket->accept ( owner );

    if ( ! socket )
        return 0;

    auto it = _pendingClients.begin();

    for ( ; it != _pendingClients.end(); ++it )
    {
        if ( it->second.address == socket->address )
            break;
    }

    if ( it != _pendingClients.end() )
    {
        _pendingTimers.erase ( it->second.timer.get() );

        _pendingClients.erase ( it );

        if ( _pendingClients.empty() && _pendingTimers.empty() )
            _sendTimer.reset();
    }

    return socket;
}

#define BOILERPLATE_SEND(...)                                                           \
    do {                                                                                \
        if ( ! isConnected() )                                                          \
            return false;                                                               \
        if ( _directSocket && _directSocket->isConnected() )                            \
            return _directSocket->send ( __VA_ARGS__ );                                 \
        if ( _tunSocket && _tunSocket->isConnected() )                                  \
            return _tunSocket->send ( __VA_ARGS__ );                                    \
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
