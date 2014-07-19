#pragma once

#include "Protocol.h"
#include "Timer.h"

#include <list>


struct AckSequence : public SerializableSequence
{
    AckSequence() {}

    AckSequence ( uint32_t sequence ) : SerializableSequence ( sequence ) {}

    MsgType getMsgType() const override;
    void save ( cereal::BinaryOutputArchive& ar ) const override {}
    void load ( cereal::BinaryInputArchive& ar ) override {}
};


class GoBackN : public Timer::Owner
{
public:

    struct Owner
    {
        // Send a message via raw socket
        virtual void sendRaw ( GoBackN *gbn, const MsgPtr& msg ) = 0;

        // Receive a message from GoBackN
        virtual void recvGoBackN ( GoBackN *gbn, const MsgPtr& msg ) = 0;

        // Timeout GoBackN if keep alive is enabled
        inline virtual void timeoutGoBackN ( GoBackN *gbn ) {};
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

    // The timeout for keep alive packets, 0 to disable
    uint64_t keepAlive, countDown;

    // Timer callback that sends the messages
    void timerExpired ( Timer *timer ) override;

public:

    // Constructor
    GoBackN ( Owner *owner, uint64_t timeout = 0 );

    // Send a message via GoBackN
    void sendGoBackN ( SerializableSequence *message );
    void sendGoBackN ( const MsgPtr& msg );

    // Receive a message from the raw socket
    void recvRaw ( const MsgPtr& msg );

    // Get/set the timeout for keep alive packets, 0 to disable
    inline uint64_t getKeepAlive() const { return keepAlive; }
    void setKeepAlive ( uint64_t timeout );

    // Get the number of messages sent and received
    inline uint32_t getSendCount() const { return sendSequence; }
    inline uint32_t getRecvCount() const { return recvSequence; }

    // Get the number of messages ACKed
    inline uint32_t getAckCount() const { return ackSequence; }

    // Reset the state of GoBackN
    void reset();
};
