#include "ControllerManager.h"
#include "Log.h"

#include <SDL.h>

using namespace std;

void ControllerManager::updateControllers()
{
    SDL_JoystickUpdate();
}

void ControllerManager::check()
{
    if ( !initialized )
        return;

    updateControllers();

    for ( const auto& kv : allocatedControllers )
    {
        if ( activeControllers.find ( kv.first ) != activeControllers.end() )
            continue;

        LOG ( "Added controller %s", kv.first );
        activeControllers[kv.first] = kv.second.get();
    }

    for ( auto it = activeControllers.begin(); it != activeControllers.end(); )
    {
        if ( allocatedControllers.find ( it->first ) != allocatedControllers.end() )
        {
            ++it;
            continue;
        }

        LOG ( "Removed controller %s", it->first ); // Don't log any extra data cus already de-allocated
        activeControllers.erase ( it++ );
    }

    // LOG ( "%d joysticks", SDL_NumJoysticks() );
}

void ControllerManager::clear()
{
    LOG ( "Clearing controllers" );
    activeControllers.clear();
    allocatedControllers.clear();
}

ControllerManager::ControllerManager() : initialized ( false ) {}

void ControllerManager::initialize ( Owner *owner )
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
        throw "something"; // TODO
    }

    // Initialize SDL joystick
    if ( SDL_JoystickEventState ( SDL_QUERY ) < 0 )
    {
        LOG ( "SDL_JoystickEventState failed: '%s'", SDL_GetError() );
        throw "something"; // TODO
    }

    updateControllers();

    this->owner = owner;
    initialized = true;
}

void ControllerManager::deinitialize()
{
    if ( !initialized )
        return;

    SDL_Quit();
    initialized = false;
}

ControllerManager& ControllerManager::get()
{
    static ControllerManager jm;
    return jm;
}
