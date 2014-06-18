#pragma once

struct Timer
{
    struct Owner
    {
        virtual void timerExpired ( Timer *timer ) {}
    };

private:

    Owner *owner;
    long delay, expiry;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( long delay );

    friend class EventManager;
};
