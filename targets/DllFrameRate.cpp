#include "DllFrameRate.h"
#include "TimerManager.h"
#include "Constants.h"
#include "ProcessManager.h"
#include "AsmHacks.h"

#include <d3dx9.h>

using namespace std;
using namespace DllFrameRate;


namespace DllFrameRate
{

uint8_t desiredFps = 60;

double actualFps = 60.0;

bool isEnabled = false;


void enable()
{
    if ( isEnabled )
        return;

    // TODO find an alternative because this doesn't work on Wine
    WRITE_ASM_HACK ( AsmHacks::disableFpsLimit );
    WRITE_ASM_HACK ( AsmHacks::disableFpsCounter );

    isEnabled = true;

    LOG ( "Enabling alternate FPS control!" );
}

}


void PresentFrameEnd ( IDirect3DDevice9 *device )
{
    if ( !isEnabled || *CC_SKIP_FRAMES_ADDR )
        return;

    static uint64_t last1f = 0, last5f = 0, last30f = 0, last60f = 0;
    static uint8_t counter = 0;

    ++counter;

    uint64_t now = TimerManager::get().getNow ( true );

    /**
     * The best timer resolution is only in milliseconds, and we need to make
     * sure the spacing between frames is as close to even as possible.
     *
     * What this code does is check every 30f, 5f, and 1f how many milliseconds have
     * passed since the last check and make sure we are close to or under the desired FPS.
     */
    if ( counter % 30 == 0 )
    {
        while ( now - last30f < ( 30 * 1000 ) / desiredFps )
            now = TimerManager::get().getNow ( true );

        last30f = now;
    }
    else if ( counter % 5 == 0 )
    {
        while ( now - last5f < ( 5 * 1000 ) / desiredFps )
            now = TimerManager::get().getNow ( true );

        last5f = now;
    }
    else
    {
        while ( now - last1f < 1000 / desiredFps )
            now = TimerManager::get().getNow ( true );
    }

    last1f = now;

    if ( counter >= 60 )
    {
        now = TimerManager::get().getNow ( true );

        actualFps = 1000.0 / ( ( now - last60f ) / 60.0 );

        *CC_FPS_COUNTER_ADDR = uint32_t ( actualFps + 0.5 );

        counter = 0;
        last60f = now;
    }
}
