#pragma once

#include <string>

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

    const uint64_t& now() const;

    inline bool isStarted() const { return ( delay > 0 || expiry > 0 ); }

    static std::string formatTimer ( const Timer *timer );

    friend class EventManager;
};
