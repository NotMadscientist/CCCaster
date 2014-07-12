#include "Event.h"
#include "Log.h"

#include <SDL.h>

using namespace std;

void EventManager::checkJoysticks()
{
    SDL_PumpEvents();
}

void EventManager::clearJoysticks()
{
    LOG ( "clearJoysticks" );
}

void EventManager::initializeJoysticks()
{
    if ( initializedJoysticks )
        return;

    // Initialize SDL
    SDL_SetHint ( SDL_HINT_XINPUT_ENABLED, "0" );
    SDL_SetHint ( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );
    if ( SDL_Init ( SDL_INIT_VIDEO ) < 0 )
        LOG ( "Could not initialize SDL: '%s'", SDL_GetError() );

    // Initialize SDL joystick
    SDL_InitSubSystem ( SDL_INIT_JOYSTICK );
    SDL_JoystickEventState ( SDL_ENABLE );

    initializedJoysticks = true;
}

void EventManager::deinitializeJoysticks()
{
    if ( !initializedJoysticks )
        return;

    SDL_Quit();
    initializedJoysticks = false;
}
