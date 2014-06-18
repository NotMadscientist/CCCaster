#pragma once

struct Timer
{
    struct Owner
    {
        virtual void timerExpired ( Timer *timer ) {}
    };

    Owner *owner;

private:

    long delay, expiry;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( long delay );

    friend class EventManager;
};
