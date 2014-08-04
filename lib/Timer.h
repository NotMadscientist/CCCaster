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

private:

    uint64_t delay = 0, expiry = 0;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( uint64_t delay );

    void stop();

    inline uint64_t getDelay() const { return delay; }

    inline bool isStarted() const { return ( delay > 0 || expiry > 0 ); }

    friend class TimerManager;
};

typedef std::shared_ptr<Timer> TimerPtr;
