#pragma once

#include "IpAddrPort.h"
#include "GoBackN.h"

#include <vector>
#include <memory>


#define LOG_SOCKET(SOCKET, FORMAT, ...)                                                                         \
    LOG ( "%s socket=%08x; fd=%08x; state=%s; address='%s'; " FORMAT,                                           \
          SOCKET->protocol, SOCKET, SOCKET->fd, SOCKET->state, SOCKET->address, ## __VA_ARGS__ )


// Forward declarations
struct _WSAPROTOCOL_INFOA;
typedef struct _WSAPROTOCOL_INFOA WSAPROTOCOL_INFO;
class Socket;
class TcpSocket;
class UdpSocket;
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
        virtual void acceptEvent ( Socket *serverSocket ) = 0;

        // Socket connected event
        virtual void connectEvent ( Socket *socket ) = 0;

        // Socket disconnected event
        virtual void disconnectEvent ( Socket *socket ) = 0;

        // Socket protocol message read event (only called if NOT isRaw)
        virtual void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) = 0;

        // Socket raw data read event (only called if isRaw)
        virtual void readEvent ( Socket *socket, const char *buffer, size_t len, const IpAddrPort& address ) {};
    };

    // Raw socket type flag
    const bool isRaw = false;

    // Socket protocol
    ENUM ( Protocol, TCP, UDP );

    // Connection state
    ENUM ( State, Listening, Connecting, Connected, Disconnected );

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

    // Check sum fail percentage for testing purposes
    uint8_t checkSumFail = 0;

    // TCP event callbacks
    virtual void acceptEvent() {}
    virtual void connectEvent() {}
    virtual void disconnectEvent() {}

    // Read event callback, calls the function below if NOT isRaw
    virtual void readEvent();

    // Read protocol message callback, must be implemented, only called if NOT isRaw
    virtual void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) = 0;

public:

    // Create a socket from SocketShareData
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Basic constructors
    Socket ( const IpAddrPort& address, Protocol protocol, bool isRaw = false );

    // Virtual destructor
    virtual ~Socket();

    // Completely disconnect the socket
    virtual void disconnect();

    // Initialize a socket with the provided address and protocol
    void init();

    // Socket state query functions
    bool isTCP() const { return ( protocol == Protocol::TCP ); }
    bool isUDP() const { return ( protocol == Protocol::UDP ); }
    virtual State getState() const { return state; }
    virtual bool isConnecting() const { return isClient() && ( state == State::Connecting ); }
    virtual bool isConnected() const { return isClient() && ( state == State::Connected ); }
    virtual bool isDisconnected() const { return ( state == State::Disconnected ); }
    virtual bool isClient() const { return !address.addr.empty(); }
    virtual bool isServer() const { return address.addr.empty(); }
    virtual const IpAddrPort& getRemoteAddress() const { if ( isServer() ) return NullAddress; return address; }

    // Send raw bytes directly, a return value of false indicates socket is disconnected
    bool send ( const char *buffer, size_t len );
    bool send ( const char *buffer, size_t len, const IpAddrPort& address );

    // Read raw bytes directly, a return value of false indicates socket is disconnected
    bool recv ( char *buffer, size_t& len );
    bool recv ( char *buffer, size_t& len, IpAddrPort& address );

    // Accept a new socket
    virtual SocketPtr accept ( Owner *owner ) = 0;

    // Get the data needed to share this socket with another process
    virtual MsgPtr share ( int processId );

    // Send a protocol message, returning false indicates the socket is disconnected
    virtual bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) = 0;
    virtual bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) = 0;

    // Get/set the interval to send packets, should be non-zero, only valid for UDP sockets
    virtual uint64_t getSendInterval() const { return 0; }
    virtual void setSendInterval ( uint64_t interval ) {}

    // Get/set the timeout for keep alive packets, 0 to disable, only valid for UDP sockets
    virtual uint64_t getKeepAlive() const { return 0; }
    virtual void setKeepAlive ( uint64_t timeout ) {}

    // Set the packet loss for testing purposes
    void setPacketLoss ( uint8_t percentage ) { packetLoss = percentage; }

    // Set the check sum fail percentage for testing purposes
    void setCheckSumFail ( uint8_t percentage ) { checkSumFail = percentage; }

    // Cast this to another socket type
    TcpSocket& getAsTCP();
    const TcpSocket& getAsTCP() const;
    UdpSocket& getAsUDP();
    const UdpSocket& getAsUDP() const;

    friend class SocketManager;
};


// Contains data for sharing a socket across processes
struct SocketShareData : public SerializableSequence
{
    IpAddrPort address;
    Socket::Protocol protocol;
    Socket::State state;
    std::string readBuffer;
    size_t readPos = 0;
    std::shared_ptr<WSAPROTOCOL_INFO> info;

    // Extra data for UDP sockets
    uint8_t udpType = 0;
    MsgPtr gbnState;
    std::unordered_map<IpAddrPort, GoBackN> childSockets;

    SocketShareData ( const IpAddrPort& address,
                      Socket::Protocol protocol,
                      Socket::State state,
                      const std::string& readBuffer,
                      size_t readPos,
                      const std::shared_ptr<WSAPROTOCOL_INFO>& info )
        : address ( address ), protocol ( protocol ), state ( state )
        , readBuffer ( readBuffer ), readPos ( readPos ), info ( info ) {}

    bool isTCP() const { return ( protocol == Socket::Protocol::TCP ); }
    bool isUDP() const { return ( protocol == Socket::Protocol::UDP ); }

    DECLARE_MESSAGE_BOILERPLATE ( SocketShareData )
};
