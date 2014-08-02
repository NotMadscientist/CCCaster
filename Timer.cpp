#include "TimerManager.h"
#include "Timer.h"

using namespace std;


Timer::Timer ( Owner *owner ) : owner ( owner ), delay ( 0 ), expiry ( 0 )
{
    TimerManager::get().add ( this );
}

Timer::~Timer()
{
    TimerManager::get().remove ( this );
}

void Timer::start ( double delay )
{
    if ( delay == 0 )
        return;

    this->delay = delay;
}

void Timer::stop()
{
    delay = expiry = 0;
}
