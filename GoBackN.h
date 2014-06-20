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
        // Send a message via raw socket
        virtual void send ( const MsgPtr& msg ) {}

        // Receive a message from Go-Back-N
        virtual void recv ( const MsgPtr& msg ) {}
    };

    Owner *owner;

private:

    // Last sent and received sequences
    uint32_t sendSequence, recvSequence;

    // Last ACKed sequence
    uint32_t ackSequence;

    // Current list of messages to repeatedly send
    std::list<MsgPtr> sendList;

    // Current position in the sendList
    std::list<MsgPtr>::iterator sendListPos;

    // Timer for repeatedly sending messages
    Timer sendTimer;

    // Timer callback that sends the messages
    void timerExpired ( Timer *timer );

public:

    // Send a message via Go-Back-N
    void send ( SerializableSequence *message );
    void send ( const MsgPtr& msg );

    // Receive a message from the raw socket
    void recv ( const MsgPtr& msg );

    // Get the number of messages sent and received
    inline uint32_t getSendCount() const { return sendSequence; }
    inline uint32_t getRecvCount() const { return recvSequence; }

    // Get the number of messages ACKed
    inline uint32_t getAckCount() const { return ackSequence; }

    // Reset the state of Go-Back-N
    void reset();

    GoBackN ( Owner *owner );
};
