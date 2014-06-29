#pragma once

#include "Socket.h"
#include "GoBackN.h"
#include "Log.h"

struct ReliableUdpConnect : public SerializableSequence
{
    ReliableUdpConnect() {}
    MsgType getType() const override;
protected:
    void serialize ( cereal::BinaryOutputArchive& ar ) const override {}
    void deserialize ( cereal::BinaryInputArchive& ar ) override {}
};

struct ReliableUdpConnected : public SerializableSequence
{
    ReliableUdpConnected() {}
    MsgType getType() const override;
protected:
    void serialize ( cereal::BinaryOutputArchive& ar ) const override {}
    void deserialize ( cereal::BinaryInputArchive& ar ) override {}
};

class ReliableUdp : public Socket
{
    // Proxy socket owner that transfers msgs via GoBackN
    struct ProxyOwner : public GoBackN::Owner, public Socket::Owner
    {
        // Parent socket
        ReliableUdp *parent;

        // Proxied socket owner
        Socket::Owner *owner;

        // GoBackN instance
        GoBackN gbn;

        // Remote address
        IpAddrPort address;

        // Constructors
        ProxyOwner ( ReliableUdp *parent, Socket::Owner *owner );
        ProxyOwner ( ReliableUdp *parent, Socket::Owner *owner, const IpAddrPort& address );

        // GoBackN callbacks
        void sendGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
        void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) override;
        void timeoutGoBackN ( GoBackN *gbn ) override;

        // Socket read callback
        void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
    };

    // Connection state
    enum class State : uint8_t { Listening, Connecting, Connected, Disconnected } state;

    // Proxy socket owners
    std::shared_ptr<ProxyOwner> proxy;
    std::unordered_map<IpAddrPort, std::shared_ptr<ProxyOwner>> proxies;

    void sendGbnAddressed ( const MsgPtr& msg, const IpAddrPort& address );
    void recvGbnAddressed ( const MsgPtr& msg, const IpAddrPort& address );

    ReliableUdp ( const std::shared_ptr<ProxyOwner>& proxy );
    ReliableUdp ( Socket::Owner *owner, unsigned port );
    ReliableUdp ( Socket::Owner *owner, const std::string& address, unsigned port );

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
    inline bool isConnected() const override { return Socket::isConnected() && ( state == State::Connected ); }

    // Send a protocol message
    void send ( Serializable *message, const IpAddrPort& address = IpAddrPort() ) override;
    void send ( const MsgPtr& msg, const IpAddrPort& address = IpAddrPort() ) override;

    // Get/set the timeout for keep alive packets, 0 to disable
    inline const uint64_t& getKeepAlive() const { return proxy->gbn.getKeepAlive(); };
    inline void setKeepAlive ( const uint64_t& timeout ) { proxy->gbn.setKeepAlive ( timeout ); };
};
