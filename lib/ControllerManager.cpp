#include "ControllerManager.h"
#include "Logger.h"

#include <SDL.h>

#include <algorithm>

using namespace std;


#define MAX_EVENT_QUEUE 64


void ControllerManager::doCheck()
{
    // TODO update keyboard controller

    SDL_PumpEvents();
    SDL_Event events[MAX_EVENT_QUEUE];

    int count = SDL_PeepEvents ( events, sizeof ( events ), SDL_GETEVENT, SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED );

    if ( count < 0 )
    {
        Exception err = toString ( "SDL error: '%s'", SDL_GetError() );
        LOG_AND_THROW_ERROR ( err, "SDL_PeepEvents failed" );
    }

    for ( int i = 0; i < count; ++i )
    {
        switch ( events[i].type )
        {
            case SDL_JOYAXISMOTION:
            {
                SDL_JoystickID id = events[i].jaxis.which;
                Controller *controller = joysticks[id].get();
                ASSERT ( controller != 0 );

                // LOG_CONTROLLER ( controller, "id=%d; SDL_JOYAXISMOTION", id );

                controller->joystickEvent ( events[i].jaxis );
                break;
            }

            case SDL_JOYHATMOTION:
            {
                SDL_JoystickID id = events[i].jhat.which;
                Controller *controller = joysticks[id].get();
                ASSERT ( controller != 0 );

                // LOG_CONTROLLER ( controller, "id=%d; SDL_JOYHATMOTION", id );

                controller->joystickEvent ( events[i].jhat );
                break;
            }

            case SDL_JOYBUTTONDOWN:
            {
                SDL_JoystickID id = events[i].jbutton.which;
                Controller *controller = joysticks[id].get();
                ASSERT ( controller != 0 );

                // LOG_CONTROLLER ( controller, "id=%d; SDL_JOYBUTTONDOWN", id );

                controller->joystickEvent ( events[i].jbutton );
                break;
            }

            case SDL_JOYBUTTONUP:
            {
                SDL_JoystickID id = events[i].jbutton.which;
                Controller *controller = joysticks[id].get();
                ASSERT ( controller != 0 );

                // LOG_CONTROLLER ( controller, "id=%d; SDL_JOYBUTTONUP", id );

                controller->joystickEvent ( events[i].jbutton );
                break;
            }

            case SDL_JOYDEVICEADDED:
            {
                SDL_Joystick *joystick = SDL_JoystickOpen ( events[i].jdevice.which );
                SDL_JoystickID id = SDL_JoystickInstanceID ( joystick );
                Controller *controller = new Controller ( joystick );
                ASSERT ( controller != 0 );

                LOG_CONTROLLER ( controller, "id=%d; SDL_JOYDEVICEADDED", id );

                joysticks[id].reset ( controller );

                if ( owner )
                    owner->attachedJoystick ( controller );

                if ( guids.find ( controller->guid.guid ) == guids.end() )
                    guids.insert ( controller->guid.guid );
                else
                    shouldReset = true;

                // LOG ( "joysticks :%s", joysticks.empty() ? " (empty)" : "" );
                // for ( const auto& kv : joysticks )
                //     LOG ( "%d -> %08x", kv.first, kv.second.get() );

                // LOG ( "Controller::guidBitset :%s", Controller::guidBitset.empty() ? " (empty)" : "" );
                // for ( const auto& kv : Controller::guidBitset )
                //     LOG ( "'%s' -> %08x", kv.first, kv.second );
                break;
            }

            case SDL_JOYDEVICEREMOVED:
            {
                SDL_JoystickID id = events[i].jdevice.which;
                Controller *controller = joysticks[id].get();
                ASSERT ( controller != 0 );

                LOG_CONTROLLER ( controller, "id=%d; SDL_JOYDEVICEREMOVED", id );

                if ( owner )
                    owner->detachedJoystick ( controller );

                if ( controller->isOnlyGuid() )
                    guids.erase ( controller->guid.guid );
                else
                    shouldReset = true;

                SDL_JoystickClose ( controller->joystick );
                joysticks.erase ( id );

                // LOG ( "joysticks :%s", joysticks.empty() ? " (empty)" : "" );
                // for ( const auto& kv : joysticks )
                //     LOG ( "%d -> %08x", kv.first, kv.second.get() );

                // LOG ( "Controller::guidBitset :%s", Controller::guidBitset.empty() ? " (empty)" : "" );
                // for ( const auto& kv : Controller::guidBitset )
                //     LOG ( "'%s' -> %08x", kv.first, kv.second );
                break;
            }

            default:
                LOG ( "Unknown event type (%d)", events[i].type );
                break;
        }
    }
}

void ControllerManager::check()
{
    if ( !initialized )
        return;

    doCheck();

    // Workaround for SDL bug 2643
    if ( shouldReset )
    {
        LOG ( "Resetting SDL!" );

        deinitialize();
        initialize ( owner );

        doCheck();

        shouldReset = false;
    }
}

void ControllerManager::clear()
{
    LOG ( "Clearing controllers" );
    joysticks.clear();
    Controller::guidBitset.clear();
}

static bool compareControllerName ( Controller *a, Controller *b )
{
    return a->name < b->name;
}

vector<Controller *> ControllerManager::getJoysticks()
{
    vector<Controller *> controllers;
    controllers.reserve ( joysticks.size() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin(), controllers.end(), compareControllerName );
    return controllers;
}

vector<Controller *> ControllerManager::getControllers()
{
    vector<Controller *> controllers;
    controllers.reserve ( joysticks.size() + 1 );

    controllers.push_back ( getKeyboard() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin() + 1, controllers.end(), compareControllerName );
    return controllers;
}

ControllerManager::ControllerManager() : keyboard ( Controller::Keyboard ) {}

void ControllerManager::initialize ( Owner *owner )
{
    this->owner = owner;

    if ( initialized )
        return;

    // Disable Xinput so 360 controllers show up with the correct name
    SDL_SetHint ( SDL_HINT_XINPUT_ENABLED, "0" );

    // Allow background joystick events
    SDL_SetHint ( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );

    // Initialize SDL joystick
    if ( SDL_Init ( SDL_INIT_JOYSTICK ) < 0 )
    {
        Exception err = toString ( "SDL error: '%s'", SDL_GetError() );
        LOG_AND_THROW_ERROR ( err, "SDL_Init(SDL_INIT_JOYSTICK) failed" );
    }

    initialized = true;
}

void ControllerManager::deinitialize()
{
    if ( !initialized )
        return;

    ControllerManager::get().clear();
    SDL_Quit();
    initialized = false;
}

ControllerManager& ControllerManager::get()
{
    static ControllerManager instance;
    return instance;
}
