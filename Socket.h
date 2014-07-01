#pragma once

#include "IpAddrPort.h"

#include <iostream>

#define LOG_SOCKET(PREFIX, SOCK)                                                                                    \
    LOG ( "%s %s socket=%08x; fd=%08x; state=%s; address='%s'",                                                     \
          PREFIX, TO_C_STR ( SOCK->protocol ), SOCK, SOCK->fd, TO_C_STR ( SOCK->state ), SOCK->address.c_str() )

class Socket
{
public:

    // Socket owner interface
    struct Owner
    {
        // Accepted a socket from server socket
        inline virtual void acceptEvent ( Socket *serverSocket ) { serverSocket->accept ( this ).reset(); }

        // Socket connected event
        inline virtual void connectEvent ( Socket *socket ) {}

        // Socket disconnected event
        inline virtual void disconnectEvent ( Socket *socket ) {}

        // Socket read event
        inline virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) {}
    };

    // Socket protocol
    enum class Protocol : uint8_t { TCP, UDP };

    // Connection state
    enum class State : uint8_t { Listening, Connecting, Connected, Disconnected };

    // Set the socket owner
    inline virtual void setOwner ( Owner *owner ) { this->owner = owner; }

private:

    // Underlying socket fd
    int fd;

    // Socket read buffer and position
    std::string readBuffer;
    size_t readPos;

    // Simulated packet loss percentage for testing
    uint8_t packetLoss;

    // Construct a client socket with an existing fd
    Socket ( Owner *owner, int fd, const std::string& address, unsigned port );

    // Initialize a socket with the provided address and protocol
    void init();

protected:

    // Socket owner
    Owner *owner;

    // Connection state
    State state;

    // Socket address
    const IpAddrPort address;

    // Socket protocol
    const Protocol protocol;

    // Construct a server socket
    Socket ( Owner *owner, unsigned port, Protocol protocol );

    // Construct a client socket
    Socket ( Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    // Construct a proxy socket (for UDP server clients)
    Socket ( Owner *owner, const std::string& address, unsigned port );

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Owner *owner, unsigned port, Protocol protocol );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect (
        Owner *owner, const std::string& address, unsigned port, Protocol protocol );

    // Virtual destructor
    virtual ~Socket();

    // Completely disconnect the socket
    virtual void disconnect();

    // Accept a new socket
    virtual std::shared_ptr<Socket> accept ( Owner *owner );

    // Socket status
    inline virtual State getState() const { return state; }
    inline virtual bool isConnected() const { return isClient() && ( fd != 0 ) && ( state == State::Connected ); }
    inline virtual bool isClient() const { return !address.addr.empty(); }
    inline virtual bool isServer() const { return address.addr.empty(); }

    // Socket parameters
    inline virtual const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }
    inline virtual Protocol getProtocol() const { return protocol; }

    // Send a protocol message
    virtual bool send ( Serializable *message, const IpAddrPort& address = IpAddrPort() );
    virtual bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() );

    // Send raw bytes directly
    bool send ( const char *buffer, size_t len, const IpAddrPort& address = IpAddrPort() );

    // Read raw bytes directly
    bool recv ( char *buffer, size_t& len );
    bool recv ( char *buffer, size_t& len, IpAddrPort& address );

    // Set the packet loss for testing purposes
    void setPacketLoss ( uint8_t percentage );

    // Get the number of sockets pending acceptance for testing purposes
    inline virtual size_t getPendingCount() const { return 0; };

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, Socket::Protocol protocol );
std::ostream& operator<< ( std::ostream& os, Socket::State state );

const char *inet_ntop ( int af, const void *src, char *dst, size_t size );
