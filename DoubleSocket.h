#pragma once

#include "Socket.h"
#include "Socket.h"
#include "GoBackN.h"
#include "Protocol.h"

#include <memory>
#include <unordered_map>
#include <cassert>

struct PrimaryId : public SerializableSequence
{
    uint32_t id;

    PrimaryId() : id ( 0 ) {}

    PrimaryId ( uint32_t id ) : id ( id ) {}

    MsgType type() const;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( id ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( id ); }
};

struct DoubleSocket : public GoBackN::Owner, public Socket::Owner
{
    struct Owner
    {
        virtual void acceptEvent ( DoubleSocket *serverSocket ) {}
        virtual void connectEvent ( DoubleSocket *socket ) {}
        virtual void disconnectEvent ( DoubleSocket *socket ) {}
        virtual void readEvent ( DoubleSocket *socket, const MsgPtr& msg, bool primary ) {}
    };

    enum class State { Listening, Connecting, Connected, Disconnected };

    Owner *owner;

private:

    // Connection state
    State state;

    // Underlying sockets
    std::shared_ptr<Socket> primary, secondary;

    // Pending accepted sockets
    std::unordered_map<IpAddrPort, std::shared_ptr<Socket>> pendingAccepts;

    // Currently accepted socket
    std::shared_ptr<DoubleSocket> acceptedSocket;

    // Map of secondary socket port to accepted socket
    std::unordered_map<unsigned, DoubleSocket *> secondaryPorts;

    // Go-Back-N
    GoBackN gbn;

    // Socket event callbacks
    void acceptEvent ( Socket *serverSocket );
    void connectEvent ( Socket *socket );
    void disconnectEvent ( Socket *socket );
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address );

    // Go-Back-N event callbacks
    void sendGoBackN ( const MsgPtr& msg );
    void recvGoBackN ( const MsgPtr& msg );

    DoubleSocket ( Owner *owner, unsigned port );
    DoubleSocket ( Owner *owner, const std::string& address, unsigned port );
    DoubleSocket ( const std::shared_ptr<Socket>& primary, const std::shared_ptr<Socket>& secondary );

public:

    // Listen for connections on the given port
    static std::shared_ptr<DoubleSocket> listen ( Owner *owner, unsigned port );

    // Connect to the given address and port
    static std::shared_ptr<DoubleSocket> connect ( Owner *owner, const std::string& address, unsigned port );

    // Connect to the given room via the relay server
    static std::shared_ptr<DoubleSocket> relay (
        Owner *owner, const std::string& room, const std::string& server, unsigned port );

    ~DoubleSocket();

    // Completely disconnect the socket
    void disconnect();

    // Accept a new socket
    std::shared_ptr<DoubleSocket> accept ( Owner *owner );

    inline bool isConnected() const { return ( primary.get() && secondary.get() && state == State::Connected ); }
    inline bool isServer() const { return primary->isServer(); }

    inline const IpAddrPort& getRemoteAddress() const { return primary->getRemoteAddress(); }

    void sendPrimary ( const MsgPtr& msg );
    void sendPrimary ( SerializableSequence *msg );

    void sendSecondary ( const MsgPtr& msg );
    void sendSecondary ( SerializableMessage *msg );
};

std::ostream& operator<< ( std::ostream& os, DoubleSocket::State state );
