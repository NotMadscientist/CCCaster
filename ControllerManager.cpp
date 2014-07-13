#include "ControllerManager.h"
#include "Logger.h"

#include <SDL.h>

#include <cassert>

using namespace std;

void ControllerManager::check()
{
    if ( !initialized )
        return;

    // TODO update keyboard controller

    SDL_PumpEvents();
    SDL_Event e;

    for ( ;; )
    {
        int ret = SDL_PeepEvents ( &e, 1, SDL_GETEVENT, SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED );

        if ( ret < 0 )
        {
            LOG ( "SDL_PeepEvents failed: '%s'", SDL_GetError() );
            throw "something"; // TODO
        }

        if ( ret == 0 )
            break;

        assert ( ret == 1 );

        switch ( e.type )
        {
            case SDL_JOYAXISMOTION:
            {
                SDL_JoystickID id = e.jaxis.which;
                Controller *controller = joysticks[id].get();
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYAXISMOTION" );

                controller->joystickEvent ( e.jaxis );
                break;
            }

            case SDL_JOYHATMOTION:
            {
                SDL_JoystickID id = e.jhat.which;
                Controller *controller = joysticks[id].get();
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYHATMOTION" );

                controller->joystickEvent ( e.jhat );
                break;
            }

            case SDL_JOYBUTTONDOWN:
            {
                SDL_JoystickID id = e.jbutton.which;
                Controller *controller = joysticks[id].get();
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYBUTTONDOWN" );

                controller->joystickEvent ( e.jbutton );
                break;
            }

            case SDL_JOYBUTTONUP:
            {
                SDL_JoystickID id = e.jbutton.which;
                Controller *controller = joysticks[id].get();
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYBUTTONUP" );

                controller->joystickEvent ( e.jbutton );
                break;
            }

            case SDL_JOYDEVICEADDED:
            {
                SDL_Joystick *stick = SDL_JoystickOpen ( e.jdevice.which );
                SDL_JoystickID id = SDL_JoystickInstanceID ( stick );
                Controller *controller = new Controller ( stick );
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYDEVICEADDED" );

                joysticks[id] = shared_ptr<Controller> ( controller );

                if ( owner )
                    owner->attachedJoystick ( controller );

                LOG ( "Controller::guidBitset :%s", Controller::guidBitset.empty() ? " (empty)" : "" );
                for ( const auto& kv : Controller::guidBitset )
                    LOG ( "'%s' -> %08x", kv.first, kv.second );
                break;
            }

            case SDL_JOYDEVICEREMOVED:
            {
                SDL_JoystickID id = e.jdevice.which;
                Controller *controller = joysticks[id].get();
                assert ( controller != 0 );

                LOG_CONTROLLER ( controller, "SDL_JOYDEVICEREMOVED" );

                if ( owner )
                    owner->detachedJoystick ( controller );

                SDL_JoystickClose ( controller->stick );
                joysticks.erase ( id );

                LOG ( "Controller::guidBitset :%s", Controller::guidBitset.empty() ? " (empty)" : "" );
                for ( const auto& kv : Controller::guidBitset )
                    LOG ( "'%s' -> %08x", kv.first, kv.second );
                break;
            }

            default:
                LOG ( "Unknown event type (%d)", e.type );
                break;
        }
    }
}

void ControllerManager::clear()
{
    LOG ( "Clearing controllers" );
    joysticks.clear();
}

ControllerManager::ControllerManager() : keyboard ( Controller::Keyboard ), initialized ( false ) {}

void ControllerManager::initialize ( Owner *owner )
{
    if ( initialized )
        return;

    // Disable Xinput so 360 controllers show up with the correct name
    SDL_SetHint ( SDL_HINT_XINPUT_ENABLED, "0" );

    // Allow background joystick events
    SDL_SetHint ( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );

    // Initialize SDL joystick
    if ( SDL_Init ( SDL_INIT_JOYSTICK ) < 0 )
    {
        LOG ( "SDL_Init(SDL_INIT_JOYSTICK) failed: '%s'", SDL_GetError() );
        throw "something"; // TODO
    }

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
