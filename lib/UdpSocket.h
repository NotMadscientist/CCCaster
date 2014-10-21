#pragma once

#include "Socket.h"
#include "GoBackN.h"


#define DEFAULT_KEEP_ALIVE ( 1000 )


struct UdpConnect : public SerializableSequence
{
    ENUM_MESSAGE_BOILERPLATE ( UdpConnect, Request, Reply, Final )
};


class UdpSocket : public Socket, public GoBackN::Owner
{
public:

    // UDP socket type enum
    ENUM ( Type, ConnectionLess, Client, Server, Child );

    // UDP socket type constant
    const Type type = Type::Unknown;

private:

    // UDP child socket enum type for choosing the right constructor
    enum ChildSocketEnum { ChildSocket };

    // GoBackN instance
    GoBackN gbn;

    // Parent socket
    UdpSocket *parentSocket = 0;

    // Child sockets
    std::unordered_map<IpAddrPort, SocketPtr> childSockets;

    // Currently accepted socket
    SocketPtr acceptedSocket;

    // GoBackN callbacks
    void sendRaw ( GoBackN *gbn, const MsgPtr& msg ) override;
    void recvRaw ( GoBackN *gbn, const MsgPtr& msg ) override;
    void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void timeoutGoBackN ( GoBackN *gbn ) override;

    // Callback into the correctly addressed socket
    void readEventAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    // Send a protocol message directly, not over GoBackN
    bool sendRaw ( const MsgPtr& msg, const IpAddrPort& address );

    // Construct a server socket
    UdpSocket ( Socket::Owner *owner, uint16_t port, const Type& type, bool isRaw = false );

    // Construct a client socket
    UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, const Type& type, bool isRaw = false );

    // Construct a socket from SocketShareData
    UdpSocket ( Socket::Owner *owner, const SocketShareData& data );

    // Construct a child socket from the parent socket
    UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address );

    // Construct a child socket from GoBackN state
    UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address, const GoBackN& state );

protected:

    // Socket read event callback
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Listen for connections on the given port
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address );

    // Create connection-less sockets
    static SocketPtr bind ( Socket::Owner *owner, uint16_t port, bool isRaw = false );
    static SocketPtr bind ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw = false );

    // Create a socket from SocketShareData
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Destructor
    ~UdpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // If this UDP socket is backed by a real socket handle, and not proxy of another socket
    bool isReal() const { return ( type == Type::ConnectionLess || type == Type::Client || type == Type::Server ); }

    // If this is a vanilla connection-less UDP socket
    bool isConnectionLess() const { return ( type == Type::ConnectionLess ); }

    // Child UDP sockets aren't real sockets, they are just proxies that recv from the parent socket
    bool isChild() const { return ( type == Type::Child ); }

    // Get the map of address to child socket
    std::unordered_map<IpAddrPort, SocketPtr>& getChildSockets() { return childSockets; }

    // Get the data needed to share this socket with another process.
    // Child UDP sockets CANNOT be shared, the parent SocketShareData contains all the child sockets.
    // The child sockets can be restored via getChildSockets after the parent socket is constructed.
    MsgPtr share ( int processId );

    // Send a protocol message, return false indicates disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;

    // Get/set the interval to send packets, should be non-zero
    virtual uint64_t getSendInterval() const { return gbn.getSendInterval(); }
    virtual void setSendInterval ( uint64_t interval ) { if ( !isConnectionLess() ) gbn.setSendInterval ( interval ); }

    // Get/set the timeout for keep alive packets, 0 to disable
    uint64_t getKeepAlive() const { return gbn.getKeepAlive(); }
    void setKeepAlive ( uint64_t timeout ) { if ( !isConnectionLess() ) gbn.setKeepAlive ( timeout ); }

    // Reset the state of the GoBackN instance
    void resetGbnState() { gbn.reset(); }
};
