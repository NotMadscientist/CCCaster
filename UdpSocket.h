#pragma once

#include "Socket.h"
#include "GoBackN.h"

struct UdpConnect : public SerializableSequence
{
    enum class ConnectType : uint8_t { Request, Reply, Final } connectType;

    UdpConnect() {}

    UdpConnect ( ConnectType connectType ) : connectType ( connectType ) {}

    MsgType getMsgType() const override;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const override { ar ( connectType ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) override { ar ( connectType ); }
};

class UdpSocket : public Socket, public GoBackN::Owner
{
    // Indicates this is a child UDP socket, and has a parent socket
    const bool hasParent;

    // Parent socket
    UdpSocket *parent;

    // GoBackN instance
    GoBackN gbn;

    // Accepted sockets
    std::unordered_map<IpAddrPort, std::shared_ptr<Socket>> acceptedSockets;

    // Currently accepted socket
    std::shared_ptr<Socket> acceptedSocket;

    // GoBackN callbacks
    void sendGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void timeoutGoBackN ( GoBackN *gbn ) override;

    // GoBackN recv convenience function
    void gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    // Send a protocol message directly, not over GoBackN
    bool sendDirect ( const MsgPtr& msg, const IpAddrPort& address );

    // Construct a server socket
    UdpSocket ( Socket::Owner *owner, uint16_t port, uint64_t keepAlive );

    // Construct a client socket
    UdpSocket ( Socket::Owner *owner, const IpAddrPort& address, uint64_t keepAlive );

    // Construct a child socket
    UdpSocket ( UdpSocket *parent, const IpAddrPort& address );

protected:

    // Socket read event callback
    void readEvent ( const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect ( Socket::Owner *owner, const IpAddrPort& address );

    // Create connection-less sockets
    static std::shared_ptr<Socket> bind ( Socket::Owner *owner, uint16_t port );
    static std::shared_ptr<Socket> bind ( Socket::Owner *owner, const IpAddrPort& address );

    // Destructor
    ~UdpSocket() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    std::shared_ptr<Socket> accept ( Socket::Owner *owner ) override;

    // Indicates this is a child UDP socket
    inline bool isChild() const { return !hasParent; }

    // Send a protocol message, return false indicates disconnected
    bool send ( SerializableMessage *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( SerializableSequence *message, const IpAddrPort& address = IpAddrPort() ) override;
    bool send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;

    // Get/set the timeout for keep alive packets, 0 to disable
    inline uint64_t getKeepAlive() const { return gbn.getKeepAlive(); };
    inline void setKeepAlive ( uint64_t timeout ) { gbn.setKeepAlive ( timeout ); };
};
