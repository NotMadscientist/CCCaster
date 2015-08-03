#pragma once

#include "Socket.hpp"
#include "GoBackN.hpp"


#define DEFAULT_KEEP_ALIVE_TIMEOUT ( 20000 )


struct UdpControl : public SerializableSequence
{
    ENUM_BOILERPLATE ( UdpControl, ConnectRequest, ConnectReply, ConnectFinal, Disconnect )

    PROTOCOL_MESSAGE_BOILERPLATE ( UdpControl, value )
};


class UdpSocket
    : public Socket
    , private GoBackN::Owner
{
public:

    // UDP socket type enum
    ENUM ( Type, ConnectionLess, Client, Server, Child );

    // Listen for connections on the given port
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address,
                               uint64_t connectTimeout = DEFAULT_CONNECT_TIMEOUT );

    // Create connection-less sockets
    static SocketPtr bind ( Socket::Owner *owner, uint16_t port, bool isRaw = false );
    static SocketPtr bind ( Socket::Owner *owner, const IpAddrPort& address, bool isRaw = false );

    // Create a socket from SocketShareData
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Destructor
    ~UdpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket, returns 0 if no socket to accept
    SocketPtr accept ( Socket::Owner *owner ) override;

    // If this UDP socket is backed by a real socket handle, and not proxy of another socket
    bool isReal() const { return ( _type == Type::ConnectionLess || _type == Type::Client || _type == Type::Server ); }

    // Child UDP sockets aren't real sockets, they are just proxies that recv from the parent socket
    bool isChild() const { return ( _type == Type::Child ); }

    // If this is a vanilla connection-less UDP socket
    bool isConnectionLess() const { return ( _type == Type::ConnectionLess ); }

    // If this is a connection-based UDP socket
    bool isConnectionBased() const { return ( _type == Type::Client || _type == Type::Child ); }

    // Get the map of address to child socket
    std::unordered_map<IpAddrPort, SocketPtr>& getChildSockets() { return _childSockets; }

    // Get the data needed to share this socket with another process.
    // Child UDP sockets CANNOT be shared, the parent SocketShareData contains all the child sockets.
    // The child sockets can be restored via getChildSockets after the parent socket is constructed.
    MsgPtr share ( int processId );

    // Send a protocol message, a return value of false indicates socket is disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = NullAddress ) override;
    bool send ( const MsgPtr& message, const IpAddrPort& address = NullAddress ) override;

    // Get / set the interval to send packets, should be non-zero
    uint64_t getSendInterval() const { return _gbn.getSendInterval(); }
    void setSendInterval ( uint64_t interval );

    // Get / set the timeout for keep alive packets, 0 to disable
    uint64_t getKeepAlive() const { return _keepAlive; }
    void setKeepAlive ( uint64_t timeout );

    // Listen for connections.
    // Can only be used on a connection-less socket, where address.addr is empty.
    // Changes the type to a message-based, UDP server socket.
    void listen();

    // Connect to the address.
    // Can only be used on a connection-less socket, where address.addr is NOT empty.
    // Changes the type to a message-based, UDP client socket.
    void connect();
    void connect ( const IpAddrPort& address );

    // Reset the state of the GoBackN instance
    void resetGbnState();

private:

    // UDP child socket enum type for choosing the right constructor
    enum ChildSocketEnum { ChildSocket };

    // UDP socket type constant
    Type _type = Type::Unknown;

    // GoBackN instance
    GoBackN _gbn;

    // Timeout for keep alive packets
    uint64_t _keepAlive = DEFAULT_KEEP_ALIVE_TIMEOUT;

    // Parent socket
    UdpSocket *_parentSocket = 0;

    // Child sockets
    std::unordered_map<IpAddrPort, SocketPtr> _childSockets;

    // Currently accepted socket
    SocketPtr _acceptedSocket;

    // Socket read event callback
    void socketRead ( const MsgPtr& msg, const IpAddrPort& address ) override;

    // GoBackN callbacks
    void goBackNSendRaw ( GoBackN *gbn, const MsgPtr& msg ) override;
    void goBackNRecvRaw ( GoBackN *gbn, const MsgPtr& msg ) override;
    void goBackNRecvMsg ( GoBackN *gbn, const MsgPtr& msg ) override;
    void goBackNTimeout ( GoBackN *gbn ) override;

    // Callback into the correctly addressed socket
    void socketReadAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    // Send a protocol message directly, not over GoBackN
    bool sendRaw ( const MsgPtr& msg, const IpAddrPort& address );

    // Construct a server socket
    UdpSocket ( Socket::Owner *owner, uint16_t port, const Type& type, bool isRaw );

    // Construct a client socket
    UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, const Type& type,
                bool isRaw, uint64_t connectTimeout );

    // Construct a socket from SocketShareData
    UdpSocket ( Socket::Owner *owner, const SocketShareData& data );

    // Construct a child socket from the parent socket
    UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address );

    // Construct a child socket from GoBackN state
    UdpSocket ( ChildSocketEnum, UdpSocket *parentSocket, const IpAddrPort& address, const GoBackN& state );
};
