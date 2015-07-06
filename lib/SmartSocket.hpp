#pragma once

#include "Socket.hpp"
#include "Timer.hpp"

#include <unordered_map>


// Socket class that tries to listen / connect over the desired protocol, but automatically falls back
// to using UDP tunnel if the initial protocol fails. Queries a remote server for UDP tunnel data.
class SmartSocket : public Socket, public Socket::Owner, public Timer::Owner
{
public:
    // Listen for connections on the given port.
    // Opens a regular server socket of the given protocol, but also listens for UDP tunnel connections.
    static SocketPtr listenTCP ( Socket::Owner *owner, uint16_t port );
    static SocketPtr listenUDP ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port.
    // Tries to connect using the given protocol, with fallback to UDP tunnel.
    static SocketPtr connectTCP ( Socket::Owner *owner, const IpAddrPort& address, bool forceTunnel = false );
    static SocketPtr connectUDP ( Socket::Owner *owner, const IpAddrPort& address, bool forceTunnel = false );

    // Destructor
    ~SmartSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // If this client UDP socket is connected over the UDP tunnel
    bool isTunnel() const;

    // Send raw bytes directly, a return value of false indicates socket is disconnected
    bool send ( const char *buffer, size_t len );
    bool send ( const char *buffer, size_t len, const IpAddrPort& address );

    // Send a protocol message, a return value of false indicates socket is disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( const MsgPtr& message, const IpAddrPort& address = NullAddress ) override;

private:

    // Child UDP socket enum type for choosing the right constructor
    enum ChildSocketEnum { ChildSocket };

    // If direct socket was supposed to be TCP
    bool isDirectTCP = false;

    // Socket that tries to listen / connect directly
    SocketPtr directSocket;

    // Socket that connects to the notification and tunnel server
    SocketPtr vpsSocket;

    // Current tunnel server to try
    std::vector<IpAddrPort>::const_iterator vpsAddress;

    // Timeout for UDP tunnel match
    TimerPtr connectTimer;

    // Integer that matches the server to the client
    uint32_t matchId = 0;

    // UDP tunnel socket
    SocketPtr tunSocket;

    // Address of the server's UDP hole
    IpAddrPort tunAddress;

    // Connecting client data
    struct TunnelClient
    {
        // Integer that matches the server to the client
        uint32_t matchId = 0;

        // Timeout for the connecting client
        TimerPtr timer;

        // Address of the client's UDP hole
        IpAddrPort address;
    };

    // Connecting matchId -> client data
    std::unordered_map<uint32_t, TunnelClient> pendingClients;

    // Connecting client timer -> matchId
    std::unordered_map<Timer *, uint32_t> pendingTimers;

    // UDP tunnel send timer
    TimerPtr sendTimer;

    // True if the acceptEvent is for directSocket
    bool isDirectAccept = false;

    // Unused base socket callback
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override {}

    // Socket callbacks
    void acceptEvent ( Socket *serverSocket ) override;
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
    void readEvent ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address ) override;

    // Timer callback
    void timerExpired ( Timer *timer ) override;

    // Got a match from the tunnel server
    void gotMatch ( uint32_t matchId );

    // Got the final tunnel info from the tunnel server
    void gotTunInfo ( uint32_t matchId, const IpAddrPort& address );

    // Construct a server socket
    SmartSocket ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol );

    // Construct a client socket
    SmartSocket ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol, bool forceTunnel );

};
