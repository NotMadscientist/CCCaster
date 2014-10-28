#pragma once

#include "Socket.h"
#include "Timer.h"

#include <unordered_map>


class SmartSocket : public Socket, public Socket::Owner, public Timer::Owner
{
    // Child UDP socket enum type for choosing the right constructor
    enum ChildSocketEnum { ChildSocket };

    // Socket that tries to listen / connect directly
    SocketPtr directSocket;

    // Socket that connects to the notification and tunnel server
    SocketPtr vpsSocket;

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

    void gotMatch ( uint32_t matchId );

    void gotUdpInfo ( uint32_t matchId, const IpAddrPort& address );

    // Construct a server socket
    SmartSocket ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol );

    // Construct a client socket
    SmartSocket ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol );

public:

    // Listen for connections on the given port.
    // Opens a regular server socket of the given protocol, but also listens for UDP tunnel connections.
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port, Socket::Protocol protocol );

    // Connect to the given address and port.
    // Tries to connect using the given protocol, with fallback to UDP tunnel.
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol );

    // Destructor
    ~SmartSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // Send raw bytes directly, a return value of false indicates socket is disconnected
    bool send ( const char *buffer, size_t len );
    bool send ( const char *buffer, size_t len, const IpAddrPort& address );

    // Send a protocol message, returning false indicates the socket is disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( const MsgPtr& message, const IpAddrPort& address = NullAddress ) override;
};
