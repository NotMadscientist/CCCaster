#pragma once

#include "IpAddrPort.h"

#include <vector>
#include <memory>

#define LOG_SOCKET(MSG, SOCKET)                                                                                     \
    LOG ( "%s %s socket=%08x; fd=%08x; state=%s; address='%s'",                                                     \
          MSG, TO_C_STR ( SOCKET->protocol ), SOCKET, SOCKET->fd, TO_C_STR ( SOCKET->state ), SOCKET->address.c_str() )

// Generic socket base class
struct Socket
{
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

    // Socket address
    // For server sockets, only the port should be set to the locally bound port
    // For client sockets, this is the remote server address
    IpAddrPort address;

    // Socket protocol
    const Protocol protocol;

    // Socket owner
    Owner *owner;

protected:

    // Connection state
    State state;

    // Underlying socket fd
    int fd;

    // Socket read buffer and position
    std::string readBuffer;
    size_t readPos;

    // Packet loss percentage for testing purposes
    uint8_t packetLoss;

    // TCP event callbacks
    virtual void acceptEvent() {};
    virtual void connectEvent() {};
    virtual void disconnectEvent() {};

    // Read event callback, calls the function below
    virtual void readEvent();

    // Read protocol message callback, not optional
    virtual void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) = 0;

public:

    // Basic constructors
    Socket ( const IpAddrPort& address, Protocol protocol );

    // Virtual destructor
    virtual ~Socket();

    // Completely disconnect the socket
    virtual void disconnect();

    // Initialize a socket with the provided address and protocol
    void init();

    // Socket state query functions
    inline virtual State getState() const { return state; }
    inline virtual bool isConnected() const { return isClient() && ( fd != 0 ) && ( state == State::Connected ); }
    inline virtual bool isClient() const { return !address.addr.empty(); }
    inline virtual bool isServer() const { return address.addr.empty(); }
    inline virtual const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }

    // Send raw bytes directly, return false indicates disconnected
    bool send ( const char *buffer, size_t len );
    bool send ( const char *buffer, size_t len, const IpAddrPort& address );

    // Read raw bytes directly, return false indicates disconnected
    bool recv ( char *buffer, size_t& len );
    bool recv ( char *buffer, size_t& len, IpAddrPort& address );

    // Accept a new socket
    virtual std::shared_ptr<Socket> accept ( Owner *owner ) = 0;

    // Send a protocol message, return false indicates disconnected
    virtual bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) = 0;

    // Set the packet loss for testing purposes
    inline void setPacketLoss ( uint8_t percentage ) { packetLoss = percentage; }

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, Socket::Protocol protocol );
std::ostream& operator<< ( std::ostream& os, Socket::State state );

const char *inet_ntop ( int af, const void *src, char *dst, size_t size );
