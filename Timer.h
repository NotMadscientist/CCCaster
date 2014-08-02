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

    Owner *owner;

private:

    double delay, expiry;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( double delay );

    void stop();

    inline double getDelay() const { return delay; }

    inline bool isStarted() const { return ( delay > 0 || expiry > 0 ); }

    friend class TimerManager;
};

typedef std::shared_ptr<Timer> TimerPtr;
