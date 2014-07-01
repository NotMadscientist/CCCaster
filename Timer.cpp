#include "Event.h"
#include "Timer.h"
#include "Util.h"

using namespace std;

Timer::Timer ( Owner *owner ) : owner ( owner ), delay ( 0 ), expiry ( 0 )
{
}

Timer::~Timer()
{
    EventManager::get().removeTimer ( this );
}

void Timer::start ( const uint64_t& delay )
{
    if ( delay == 0 )
        return;

    this->delay = delay;

    EventManager::get().addTimer ( this );
}

void Timer::stop()
{
    delay = expiry = 0;

    EventManager::get().removeTimer ( this );
}

ostream& operator<< ( ostream& os, const Timer& timer )
{
    return os << toString ( "%08x:'%llu ms'", &timer, timer.getDelay() );
}
