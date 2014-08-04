#include "TimerManager.h"
#include "Timer.h"

using namespace std;


Timer::Timer ( Owner *owner ) : owner ( owner )
{
    TimerManager::get().add ( this );
}

Timer::~Timer()
{
    TimerManager::get().remove ( this );
}

void Timer::start ( uint64_t delay )
{
    if ( delay == 0 )
        return;

    this->delay = delay;
}

void Timer::stop()
{
    delay = expiry = 0;
}
