#pragma once

#include "Socket.h"
#include "Timer.h"
#include "Protocol.h"

#include <memory>
#include <list>

struct PrimaryId : public SerializableSequence
{
    uint32_t id;

    PrimaryId() : id ( 0 ) {}

    PrimaryId ( uint32_t sequence, uint32_t id ) : SerializableSequence ( sequence ), id ( id ) {}

    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( id ); }

    virtual void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( id ); }

    virtual MsgType type() const;
};

struct DoubleSocket : public Socket::Owner, public Timer::Owner
{
    struct Owner
    {
        virtual void acceptEvent ( DoubleSocket *serverSocket ) {}
        virtual void connectEvent ( DoubleSocket *socket ) {}
        virtual void disconnectEvent ( DoubleSocket *socket ) {}
        virtual void readEvent ( DoubleSocket *socket, const MsgPtr& msg, const IpAddrPort& address ) {}
    };

private:

    Owner& owner;

    std::unordered_map<Socket *, std::shared_ptr<Socket>> primaryMap;
    std::unordered_map<Socket *, std::shared_ptr<Socket>> secondaryMap;

    std::shared_ptr<Socket> primary, secondary;

    Timer resendTimer;
    std::list<MsgPtr> resendList;

    void acceptEvent ( Socket *serverSocket );
    void connectEvent ( Socket *socket );
    void disconnectEvent ( Socket *socket );
    void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address );
    void timerExpired ( Timer *timer );

    DoubleSocket ( Owner& owner, unsigned port );
    DoubleSocket ( Owner& owner, const std::string& address, unsigned port );

public:

    static DoubleSocket *listen ( Owner& owner, unsigned port );
    static DoubleSocket *connect ( Owner& owner, const std::string& address, unsigned port );
    static DoubleSocket *relay ( Owner& owner, const std::string& room, const std::string& server, unsigned port );

    ~DoubleSocket();

    void disconnect();

    DoubleSocket *accept ( Owner& owner );

    void sendPrimary   ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void sendSecondary ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
};
