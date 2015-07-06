#pragma once

#include "Socket.hpp"
#include "Timer.hpp"


class TcpSocket : public Socket, public Timer::Owner
{
    // Timeout for initial connect
    TimerPtr connectTimer;

    // Timer callback
    void timerExpired ( Timer *timer ) override;

    // Construct a server socket
    TcpSocket ( Socket::Owner *owner, uint16_t port, bool isRaw );

    // Construct a client socket
    TcpSocket ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw, uint64_t connectTimeout );

    // Construct an accepted client socket
    TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address, bool isRaw );

    // Construct a socket from SocketShareData
    TcpSocket ( Socket::Owner *owner, const SocketShareData& data );

protected:

    // Socket event callbacks
    void acceptEvent() override;
    void connectEvent() override;
    void disconnectEvent() override;
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Listen for connections on the given port
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port, bool isRaw = false );

    // Connect to the given address and port
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address,
                               bool isRaw = false, uint64_t connectTimeout = DEFAULT_CONNECT_TIMEOUT );

    // Create a socket from SocketShareData
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Destructor
    ~TcpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket, HANGS if no socket to accept
    SocketPtr accept ( Socket::Owner *owner ) override;

    // Send a protocol message, a return value of false indicates socket is disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( const MsgPtr& message, const IpAddrPort& address = NullAddress ) override;
};
