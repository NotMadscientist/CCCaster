#pragma once

#include "Protocol.h"
#include "RollingAverage.h"


struct BaseFramePing
{
    uint32_t frame;

    uint64_t timestamp;

    BaseFramePing() {};

    BaseFramePing ( uint32_t frame, uint64_t timestamp ) : frame ( frame ), timestamp ( timestamp ) {}
};

struct FramePing : public SerializableMessage, public BaseFramePing
{
    FramePing ( uint32_t frame, uint64_t timestamp ) : BaseFramePing ( frame, timestamp ) {}

    std::string str() const override { return format ( "FramePing[%u,%llu]", frame, timestamp ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( FramePing, frame, timestamp )
};

struct FramePong : public SerializableMessage, public BaseFramePing
{
    FramePong ( const FramePing& ping ) : BaseFramePing ( ping.frame, ping.timestamp ) {}

    std::string str() const override { return format ( "FramePong[%u,%llu]", frame, timestamp ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( FramePong, frame, timestamp )
};


class FrameStepPinger
{
public:

    struct Owner
    {
        virtual void sendFramePing ( FrameStepPinger *pinger, const MsgPtr& ping ) = 0;
    };

    Owner *owner = 0;

private:

    RollingAverage<uint64_t, 5> avgLatency;

    uint32_t localFrame = 0, remoteFrame = 0;

    uint64_t localPingSendTime = 0, remotePingRecvTime = 0;

    int64_t frameDelta = 0;

    void updateFrameDelta();

public:

    FrameStepPinger ( Owner *owner );

    void reset();

    void reset ( uint64_t initialLatency );

    void sendFramePing ( uint32_t frame );

    void gotFramePing ( const MsgPtr& ping );

    int64_t getFrameDelta() const { return frameDelta; }

    uint64_t getLatency() const { return avgLatency.get(); }
};
