#pragma once

#include <iostream>

class Timer
{
public:

    struct Owner
    {
        virtual void timerExpired ( Timer *timer ) = 0;
    };

    Owner *owner;

private:

    uint64_t delay, expiry;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( const uint64_t& delay );

    void stop();

    inline const uint64_t& getDelay() const { return delay; }

    inline bool isStarted() const { return ( delay > 0 || expiry > 0 ); }

    friend class EventManager;
};

std::ostream& operator<< ( std::ostream& os, const Timer *timer );
