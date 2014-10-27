#include "SmartSocket.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Logger.h"

#include <ws2tcpip.h>

using namespace std;

// TODO add more state to this log macro
#define LOG_SMART_SOCKET(SOCKET, FORMAT, ...) LOG_SOCKET ( SOCKET, FORMAT, ## __VA_ARGS__)

#define SEND_INTERVAL ( 50 )

const IpAddrPort vpsAddress = "23.95.23.238:3939";

const string matchString = "match";

const string infoString = "info";


/* Tunnel protocol

    1 - Host opens a TCP socket to the server and sends its hosting port (one uint16_t).
        Host should maintain the socket connection; reconnect and resend if needed.

    2 - Client opens a TCP socket to the server and sends an address string ("<ip>:<port>").

    2 - Server tries to matchmake:
        If a matching host if found, the server sends "match" to host AND client to signal match.
        Otherwise disconnects the client if no matching host exists.

    3 - On match, host and client both create a new UDP socket (bind any port),
        and start sending 0-byte UDP packets to server's UDP port.

    4 - Server recvs UDP packets, and relays UDP info to each side over TCP

    5 - Host and client can now connect over the UDP tunnel

*/

SmartSocket::SmartSocket ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol )
    : Socket ( owner, IpAddrPort ( "", port ), Protocol::Smart )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    this->state = State::Listening;

    vpsSocket = TcpSocket::connect ( this, vpsAddress, true ); // Raw socket

    // Listen for direct connections in parallel
    if ( protocol == Protocol::TCP )
        directSocket = TcpSocket::listen ( this, port );
    else
        directSocket = UdpSocket::listen ( this, port );
}

SmartSocket::SmartSocket ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol )
    : Socket ( owner, address, Protocol::Smart )
{
    ASSERT ( protocol != Protocol::Smart );

    freeBuffer();

    this->state = State::Connecting;

    // Try to connect normally first
    if ( protocol == Protocol::TCP )
        directSocket = TcpSocket::connect ( this, address );
    else
        directSocket = UdpSocket::connect ( this, address );
}

SmartSocket::~SmartSocket()
{
    disconnect();
}

void SmartSocket::disconnect()
{
    Socket::disconnect();

    directSocket.reset();
    vpsSocket.reset();
    matchTimer.reset();
    tunSocket.reset();
    sendTimer.reset();
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

            matchTimer.reset ( new Timer ( this ) );
            matchTimer->start ( connectTimeout );
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

        if ( isServer() )
        {
            tunSocket.reset();
            sendTimer.reset();
            return;
        }

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
    ASSERT ( socket == vpsSocket.get() );

    vpsSocket->readPos += len;
    LOG ( "Read [ %u bytes ] from '%s'; %u bytes remaining in buffer", len, address, vpsSocket->readPos );

    if ( len <= 256 )
        LOG ( "Base64 : %s", toBase64 ( buffer, len ) );

    for ( ;; )
    {
        if ( vpsSocket->readPos >= matchString.size()
                && string ( &vpsSocket->readBuffer[0], matchString.size() ) == matchString )
        {
            LOG_SMART_SOCKET ( this, "gotMatch" );

            vpsSocket->consumeBuffer ( matchString.size() );

            gotMatch();
        }
        else if ( vpsSocket->readPos >= infoString.size()
                  && string ( &vpsSocket->readBuffer[0], infoString.size() ) == infoString )
        {
            size_t start = infoString.size() + 1;
            size_t i, end = 0;

            for ( i = 0; i < 21; ++i ) // max IpAddrPort string length ("255.255.255.255:65535")
            {
                if ( start + i >= vpsSocket->readPos )
                    break;

                if ( vpsSocket->readBuffer[start + i] == '\0' )
                {
                    end = start + i;
                    break;
                }
            }

            // Failed to find null-terminator
            if ( i == 21 )
            {
                disconnectEvent ( socket );
                return;
            }

            // Not enough data
            if ( end == 0 )
                break;

            IpAddrPort address = string ( &vpsSocket->readBuffer[start], end - start );

            LOG_SMART_SOCKET ( this, "gotUdpInfo ( '%s' )", address );

            vpsSocket->consumeBuffer ( end );

            gotUdpInfo ( address );
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
    if ( timer == matchTimer.get() )
    {
        if ( isServer() )
        {
            matchTimer.reset();
            tunSocket.reset();
            sendTimer.reset();
            return;
        }

        LOG_SMART_SOCKET ( this, "No matching host found" );

        Socket::Owner *owner = this->owner;

        disconnect();

        if ( owner )
            owner->disconnectEvent ( this );
    }
    else if ( timer == sendTimer.get() )
    {
        ASSERT ( tunSocket.get() != 0 );

        tunSocket->send ( NullMsg, vpsAddress );

        sendTimer->start ( SEND_INTERVAL );
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }
}

void SmartSocket::gotMatch()
{
    if ( tunSocket )
        return;

    if ( isServer() )
        tunSocket = UdpSocket::bind ( this, address.port );
    else
        tunSocket = UdpSocket::bind ( this, vpsAddress );

    sendTimer.reset ( new Timer ( this ) );
    sendTimer->start ( SEND_INTERVAL );

    if ( !matchTimer )
        matchTimer.reset ( new Timer ( this ) );

    matchTimer->start ( connectTimeout );
}

void SmartSocket::gotUdpInfo ( const IpAddrPort& address )
{
    if ( !tunSocket )
        return;

    sendTimer.reset();

    ASSERT ( tunSocket->isUDP() == true );

    if ( isServer() )
        tunSocket->getAsUDP().listen();
    else
        tunSocket->getAsUDP().connect ( address );
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
