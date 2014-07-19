#pragma once

#include "Socket.h"
#include "GoBackN.h"


struct UdpConnect : public SerializableSequence
{
    enum class ConnectType : uint8_t { Request, Reply, Final } connectType;

    UdpConnect() {}
    UdpConnect ( ConnectType connectType ) : connectType ( connectType ) {}

    PROTOCOL_BOILERPLATE ( connectType )
};


class UdpSocket : public Socket, public GoBackN::Owner
{
    // Enum type for child sockets
    enum ChildSocketEnum { ChildSocket };

    // Indicates this is a child socket, and has a parent socket
    const bool hasParent;

    // Parent socket
    UdpSocket *parent;

    // GoBackN instance
    GoBackN gbn;

    // Child sockets
    std::unordered_map<IpAddrPort, SocketPtr> childSockets;

    // Currently accepted socket
    SocketPtr acceptedSocket;

    // GoBackN callbacks
    void sendRaw ( GoBackN *gbn, const MsgPtr& msg ) override;
    void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void timeoutGoBackN ( GoBackN *gbn ) override;

    // GoBackN recv convenience function
    void gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    // Send a protocol message directly, not over GoBackN
    bool sendRaw ( const MsgPtr& msg, const IpAddrPort& address );

    // Construct a server socket
    UdpSocket ( Socket::Owner *owner, uint16_t port, uint64_t keepAlive );

    // Construct a client socket
    UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive );

    // Construct a child socket
    UdpSocket ( ChildSocketEnum, UdpSocket *parent, const IpAddrPort& address );

    // Construct a socket from share data
    UdpSocket ( Socket::Owner *owner, const SocketShareData& data );

protected:

    // Socket read event callback
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Listen for connections on the given port
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address );

    // Create connection-less sockets
    static SocketPtr bind ( Socket::Owner *owner, uint16_t port );
    static SocketPtr bind ( Socket::Owner *owner, const IpAddrPort& address );

    // Create a socket from share data
    static SocketPtr shared ( Socket::Owner *owner, const SocketShareData& data );

    // Destructor
    ~UdpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    SocketPtr accept ( Socket::Owner *owner ) override;

    // Indicates this is a child socket
    inline bool isChild() const { return hasParent; }

    // Send a protocol message, return false indicates disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;

    // Get/set the timeout for keep alive packets, 0 to disable
    inline uint64_t getKeepAlive() const { return gbn.getKeepAlive(); };
    inline void setKeepAlive ( uint64_t timeout ) { gbn.setKeepAlive ( timeout ); };
};
