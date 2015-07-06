#pragma once

#include <iostream>
#include <memory>


class Timer
{
public:

    struct Owner
    {
        virtual void timerExpired ( Timer *timer ) = 0;
    };

    Owner *owner = 0;

    Timer ( Owner *owner );
    ~Timer();

    void start ( uint64_t delay );

    void stop();

    uint64_t getDelay() const { return delay; }

    bool isStarted() const { return ( delay > 0 || expiry > 0 ); }

    friend class TimerManager;

private:

    uint64_t delay = 0, expiry = 0;
};

typedef std::shared_ptr<Timer> TimerPtr;
