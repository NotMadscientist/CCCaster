#pragma once

#include "IpAddrPort.h"

#include <vector>
#include <memory>


#define LOG_SOCKET(SOCKET, FORMAT, ...)                                                                         \
    LOG ( "%s socket=%08x; fd=%08x; state=%s; address='%s'; " FORMAT,                                           \
          SOCKET->protocol, SOCKET, SOCKET->fd, SOCKET->state, SOCKET->address, ## __VA_ARGS__ )


// Forward declarations
struct _WSAPROTOCOL_INFOA;
typedef struct _WSAPROTOCOL_INFOA WSAPROTOCOL_INFO;
class Socket;
struct SocketShareData;

typedef std::shared_ptr<Socket> SocketPtr;


// Generic socket base class
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

    // Socket address
    // For server sockets, only the port should be set to the locally bound port
    // For client sockets, this is the remote server address
    IpAddrPort address;

    // Socket protocol
    const Protocol protocol;

    // Socket owner
    Owner *owner = 0;

protected:

    // Connection state
    State state = State::Disconnected;

    // Underlying socket fd
    int fd = 0;

    // Socket read buffer and position
    std::string readBuffer;
    size_t readPos = 0;

    // Packet loss percentage for testing purposes
    uint8_t packetLoss = 0;

    // TCP event callbacks
    virtual void acceptEvent() {};
    virtual void connectEvent() {};
    virtual void disconnectEvent() {};

    // Read event callback, calls the function below
    virtual void readEvent();

    // Read protocol message callback, not optional
    virtual void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) = 0;

public:

    // Create a socket from share data
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Basic constructors
    Socket ( const IpAddrPort& address, Protocol protocol );

    // Virtual destructor
    virtual ~Socket();

    // Completely disconnect the socket
    virtual void disconnect();

    // Initialize a socket with the provided address and protocol
    void init();

    // Socket state query functions
    inline bool isTCP() const { return ( protocol == Protocol::TCP ); }
    inline bool isUDP() const { return ( protocol == Protocol::UDP ); }
    inline virtual State getState() const { return state; }
    inline virtual bool isConnecting() const { return isClient() && ( state == State::Connecting ); }
    inline virtual bool isConnected() const { return isClient() && ( state == State::Connected ); }
    inline virtual bool isDisconnected() const { return ( state == State::Disconnected ); }
    inline virtual bool isClient() const { return !address.addr.empty(); }
    inline virtual bool isServer() const { return address.addr.empty(); }
    inline virtual const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }

    // Send raw bytes directly, a return value of false indicates socket is disconnected
    bool send ( const char *buffer, size_t len );
    bool send ( const char *buffer, size_t len, const IpAddrPort& address );

    // Read raw bytes directly, a return value of false indicates socket is disconnected
    bool recv ( char *buffer, size_t& len );
    bool recv ( char *buffer, size_t& len, IpAddrPort& address );

    // Accept a new socket
    virtual SocketPtr accept ( Owner *owner ) = 0;

    // Get data needed to share this socket in another process
    virtual MsgPtr share ( int processId );

    // Send a protocol message, returning false indicates the socket is disconnected
    virtual bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) = 0;

    // Set the packet loss for testing purposes
    inline void setPacketLoss ( uint8_t percentage ) { packetLoss = percentage; }

    friend class SocketManager;
};


// Contains data for sharing a socket across processes
struct SocketShareData : public SerializableMessage
{
    IpAddrPort address;
    Socket::Protocol protocol;
    Socket::State state;
    std::string readBuffer;
    size_t readPos;
    std::shared_ptr<WSAPROTOCOL_INFO> info;

    SocketShareData() : readPos ( 0 ) {}
    SocketShareData ( const IpAddrPort& address,
                      Socket::Protocol protocol,
                      Socket::State state,
                      const std::string& readBuffer,
                      size_t readPos,
                      const std::shared_ptr<WSAPROTOCOL_INFO>& info )
        : address ( address ), protocol ( protocol ), state ( state )
        , readBuffer ( readBuffer ), readPos ( readPos ), info ( info ) {}

    inline bool isTCP() const { return ( protocol == Socket::Protocol::TCP ); }
    inline bool isUDP() const { return ( protocol == Socket::Protocol::UDP ); }

    MsgType getMsgType() const override;
    void save ( cereal::BinaryOutputArchive& ar ) const override;
    void load ( cereal::BinaryInputArchive& ar ) override;
};


// Stream operators
std::ostream& operator<< ( std::ostream& os, Socket::Protocol protocol );
std::ostream& operator<< ( std::ostream& os, Socket::State state );
