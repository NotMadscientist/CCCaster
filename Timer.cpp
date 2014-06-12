#include "Event.h"
#include "Timer.h"

using namespace std;

Timer::Timer ( Owner& owner ) : owner ( owner ), expiry ( 0.0 )
{
}

Timer::~Timer()
{
    EventManager::get().removeTimer ( this );
}

void Timer::start ( double delay )
{
    expiry = EventManager::get().now() + delay;

    EventManager::get().addTimer ( this );
}
