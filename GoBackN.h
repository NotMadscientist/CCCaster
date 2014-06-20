#pragma once

#include "Protocol.h"
#include "Timer.h"

#include <list>

struct AckSequence : public SerializableSequence
{
    AckSequence() {}

    AckSequence ( uint32_t sequence ) : SerializableSequence ( sequence ) {}

    MsgType type() const;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const {}

    void deserialize ( cereal::BinaryInputArchive& ar ) {}
};

struct GoBackN : public Timer::Owner
{
    struct Owner
    {
        virtual void send ( const MsgPtr& msg ) {}
        virtual void recv ( const MsgPtr& msg ) {}
    };

    Owner *owner;

private:

    uint32_t sendSequence, recvSequence;

    std::list<MsgPtr> sendList, recvList;

    std::list<MsgPtr>::iterator sendListPos;

    Timer sendTimer;

    void timerExpired ( Timer *timer );

public:

    void send ( const MsgPtr& msg );

    void recv ( const MsgPtr& msg );

    void reset();

    GoBackN ( Owner *owner );
};
