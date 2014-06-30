#pragma once

#include "Socket.h"
#include "GoBackN.h"
#include "Log.h"

struct UdpConnect : public SerializableSequence
{
    enum class Type : uint8_t { Request, Reply, Final } type;

    UdpConnect() {}

    UdpConnect ( Type type ) : type ( type ) {}

    MsgType getType() const override;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const override { ar ( type ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) override { ar ( type ); }
};

class ReliableUdp : public Socket, public GoBackN::Owner, public Socket::Owner
{
    // Connection state
    enum class State : uint8_t { Listening, Connecting, Connected, Disconnected } state;

    // Parent socket
    ReliableUdp *parentSocket;

    // Proxied socket owner
    Socket::Owner *proxiedOwner;

    // GoBackN instance
    GoBackN gbn;

    // Accepted sockets
    std::unordered_map<IpAddrPort, std::shared_ptr<Socket>> acceptedSockets;

    // GoBackN callbacks
    void sendGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
    void timeoutGoBackN ( GoBackN *gbn ) override;

    // Socket read callbacks
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
    void gbnRecvAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    // Construct a server socket
    ReliableUdp ( Socket::Owner *owner, unsigned port );

    // Construct a client socket
    ReliableUdp ( Socket::Owner *owner, const std::string& address, unsigned port );

    // Construct a proxy socket (for UDP server clients)
    ReliableUdp ( ReliableUdp *parent, Socket::Owner *owner, const std::string& address, unsigned port );

public:

    // Listen for connections on the given port
    static std::shared_ptr<Socket> listen ( Socket::Owner *owner, unsigned port );

    // Connect to the given address and port
    static std::shared_ptr<Socket> connect ( Socket::Owner *owner, const std::string& address, unsigned port );

    // Destructor
    ~ReliableUdp() override;

    // Completely disconnect the socket
    void disconnect() override;

    // Accept a new socket
    std::shared_ptr<Socket> accept ( Socket::Owner *owner ) override;

    // Socket status
    inline bool isConnected() const override { return ( state == State::Connected ); }

    // Send a protocol message
    void send ( Serializable *message, const IpAddrPort& address = IpAddrPort() ) override;
    void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;

    // Get/set the timeout for keep alive packets, 0 to disable
    inline const uint64_t& getKeepAlive() const { return gbn.getKeepAlive(); };
    inline void setKeepAlive ( const uint64_t& timeout ) { gbn.setKeepAlive ( timeout ); };

    // Set the socket owner
    inline void setOwner ( Socket::Owner *owner ) { proxiedOwner = owner; }
};
