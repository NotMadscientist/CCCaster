#ifndef RELEASE

#include "EventManager.h"
#include "TimerManager.h"
#include "Timer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

using namespace std;


#define EPSILON_MILLISECONDS    50
#define NUM_ITERATIONS          10
#define MAX_DELAY_MILLISECONDS  2000


TEST ( Timer, RepeatRandom )
{
    struct TestTimer : public Timer::Owner
    {
        Timer timer;
        int count;
        uint64_t lastExpiry;
        vector<bool> validTimers;

        void timerExpired ( Timer *timer ) override
        {
            if ( lastExpiry != 0 )
            {
                validTimers.push_back ( abs ( ( long long ) ( TimerManager::get().getNow() - lastExpiry ) )
                                        < EPSILON_MILLISECONDS );
            }

            if ( count <= 0 )
            {
                EventManager::get().stop();
                return;
            }

            uint64_t delay = rand() % MAX_DELAY_MILLISECONDS;
            lastExpiry = TimerManager::get().getNow() + delay;
            timer->start ( delay );
            --count;
        }

        TestTimer() : timer ( this ), count ( NUM_ITERATIONS ), lastExpiry ( 0 )
        {
            timer.start ( 1000 );
        }
    };

    TimerManager::get().initialize();

    TestTimer test;

    EventManager::get().start();

    EXPECT_FALSE ( test.validTimers.empty() );

    EXPECT_EQ ( NUM_ITERATIONS, test.validTimers.size() );

    for ( bool valid : test.validTimers )
        EXPECT_TRUE ( valid );

    TimerManager::get().deinitialize();
}

#endif // RELEASE
