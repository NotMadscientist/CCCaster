#include "JoystickManager.h"
#include "Log.h"

#include <SDL.h>

using namespace std;

void JoystickManager::check()
{
    if ( !initialized )
        return;

    SDL_JoystickUpdate();

    // LOG ( "%d joysticks", SDL_NumJoysticks() );
}

void JoystickManager::clear()
{
    LOG ( "Clearing joysticks" );
}

JoystickManager::JoystickManager() : initialized ( false ) {}

void JoystickManager::initialize()
{
    if ( initialized )
        return;

    // Disable Xinput so 360 controllers show up with the correct name
    SDL_SetHint ( SDL_HINT_XINPUT_ENABLED, "0" );

    // Allow background joystick events
    SDL_SetHint ( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );

    // Initialize SDL
    if ( SDL_Init ( SDL_INIT_JOYSTICK ) < 0 )
    {
        LOG ( "SDL_Init failed: '%s'", SDL_GetError() );
        throw "something";
    }

    // Initialize SDL joystick
    if ( SDL_JoystickEventState ( SDL_QUERY ) < 0 )
    {
        LOG ( "SDL_JoystickEventState failed: '%s'", SDL_GetError() );
        throw "something";
    }

    initialized = true;
}

void JoystickManager::deinitialize()
{
    if ( !initialized )
        return;

    SDL_Quit();
    initialized = false;
}

JoystickManager& JoystickManager::get()
{
    static JoystickManager jm;
    return jm;
}
