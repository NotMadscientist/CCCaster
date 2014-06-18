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

    MsgType type() const;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( id ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( id ); }
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

    enum class State { Listening, Connecting, Connected, Disconnected };

private:

    Owner *owner;

    State state;

    std::unordered_map<Socket *, std::shared_ptr<Socket>> pendingAccepts;
    std::shared_ptr<DoubleSocket> acceptedSocket;

    std::shared_ptr<Socket> primary, secondary;

    Timer resendTimer;
    std::list<MsgPtr> resendList;
    std::list<MsgPtr>::iterator resendIter;
    std::unordered_set<uint32_t> resendToDel;

    void addMsgToResend ( const MsgPtr& msg );
    void removeMsgToResend ( uint32_t sequence );

    void acceptEvent ( Socket *serverSocket );
    void connectEvent ( Socket *socket );
    void disconnectEvent ( Socket *socket );
    void readEvent ( Socket *socket, char *bytes, size_t len, const IpAddrPort& address );
    void timerExpired ( Timer *timer );

    DoubleSocket ( Owner *owner, unsigned port );
    DoubleSocket ( Owner *owner, const std::string& address, unsigned port );
    DoubleSocket ( const std::shared_ptr<Socket>& primary, const std::shared_ptr<Socket>& secondary );

public:

    static DoubleSocket *listen ( Owner *owner, unsigned port );
    static DoubleSocket *connect ( Owner *owner, const std::string& address, unsigned port );
    static DoubleSocket *relay ( Owner *owner, const std::string& room, const std::string& server, unsigned port );

    ~DoubleSocket();

    void disconnect();

    std::shared_ptr<DoubleSocket> accept ( Owner *owner );

    void sendPrimary   ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
    void sendSecondary ( const Serializable& msg, const IpAddrPort& address = IpAddrPort() );
};

std::ostream& operator<< ( std::ostream& os, DoubleSocket::State state );
