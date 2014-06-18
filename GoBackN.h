#pragma once

#include "Protocol.h"
#include "Socket.h"
#include "Timer.h"

struct AckSequence : public SerializableMessage
{
    uint32_t sequence;

    AckSequence() : sequence ( 0 ) {}

    AckSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    MsgType type() const;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( sequence ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( sequence ); }
};

struct GoBackN : public Timer::Owner
{
    struct Owner
    {
    };

    Owner *owner;

private:

    void timerExpired ( Timer *timer );

public:

};
