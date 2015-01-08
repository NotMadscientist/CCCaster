#include "FrameStepPinger.h"
#include "TimerManager.h"

using namespace std;


FrameStepPinger::FrameStepPinger ( Owner *owner ) : owner ( owner )
{
    reset();
}

void FrameStepPinger::reset()
{
    avgLatency.reset();
}

void FrameStepPinger::reset ( uint64_t initialLatency )
{
    avgLatency.reset ( initialLatency );
}

void FrameStepPinger::sendFramePing ( uint32_t frame )
{
    localFrame = frame;

    localPingSendTime = TimerManager::get().getNow ( true );

    updateFrameDelta();

    if ( owner )
        owner->sendFramePing ( this, MsgPtr ( new FramePing ( frame, localPingSendTime ) ) );
}

void FrameStepPinger::gotFramePing ( const MsgPtr& ping )
{
    ASSERT ( ping->getMsgType() == MsgType::FramePing || ping->getMsgType() == MsgType::FramePong );

    const uint64_t now = TimerManager::get().getNow();

    if ( ping->getMsgType() == MsgType::FramePing )
    {
        remoteFrame = ping->getAs<FramePing>().frame;

        remotePingRecvTime = now;

        updateFrameDelta();

        if ( owner )
            owner->sendFramePing ( this, MsgPtr ( new FramePong ( ping->getAs<FramePing>() ) ) );
    }
    else if ( ping->getAs<FramePong>().frame == localFrame )
    {
        const uint64_t latency = ( now - ping->getAs<FramePong>().timestamp ) / 2;

        LOG ( "latency=%llu ms", latency );

        avgLatency.set ( latency );
    }
}

void FrameStepPinger::updateFrameDelta()
{
    if ( localFrame != remoteFrame )
        return;

    frameDelta = int64_t ( localPingSendTime ) - ( int64_t ( remotePingRecvTime ) - int64_t ( avgLatency.get() ) );

    LOG ( "frameDelta=%lld ms", frameDelta );
}
