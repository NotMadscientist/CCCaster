#include "ControllerManager.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#include <SDL.h>
#include <windows.h>

#include <algorithm>

using namespace std;


#define MAX_EVENT_QUEUE 64


uint32_t ( *ControllerManager::mapInputState ) ( uint32_t state, bool isKeyboard );


void ControllerManager::check ( void *keyboardWindowHandle )
{
    if ( !initialized )
        return;

    if ( !keyboardWindowHandle || keyboardWindowHandle == ( void * ) GetForegroundWindow() )
    {
        // Update keyboard controller state
        keyboard.state = 0;

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( !keyboard.keybd.codes[i] )
                continue;

            if ( GetKeyState ( keyboard.keybd.codes[i] ) & 0x80 )
                keyboard.state |= ( 1u << i );
        }

        if ( mapInputState )
            keyboard.state = mapInputState ( keyboard.state, true );
    }

    checkJoystick();

    // Workaround for SDL bug 2643
    if ( shouldReset )
    {
        LOG ( "Resetting SDL!" );

        deinitialize();
        initialize ( owner );

        checkJoystick();

        shouldReset = false;
    }
}

void ControllerManager::checkJoystick()
{
    // Update SDL joystick events
    SDL_PumpEvents();
    SDL_Event events[MAX_EVENT_QUEUE];

    int count = SDL_PeepEvents ( events, sizeof ( events ), SDL_GETEVENT, SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED );

    if ( count < 0 )
        THROW_SDL_EXCEPTION ( SDL_GetError(), "SDL_PeepEvents failed", ERROR_CONTROLLER_CHECK );

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
                joysticksByName[controller->getName()] = controller;

                auto it = mappings.mappings.find ( controller->getName() );
                if ( it != mappings.mappings.end() && it->second->getMsgType() == MsgType::JoystickMappings )
                    controller->setMappings ( it->second->getAs<JoystickMappings>() );

                if ( owner )
                    owner->attachedJoystick ( controller );

                // Workaround SDL bug 2643
                if ( uniqueNames.find ( controller->name ) == uniqueNames.end() )
                    uniqueNames.insert ( controller->name );
                else
                    shouldReset = true;

                // LOG ( "joysticks :%s", joysticks.empty() ? " (empty)" : "" );
                // for ( const auto& kv : joysticks )
                //     LOG ( "%d -> %08x", kv.first, kv.second.get() );
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

                // Workaround SDL bug 2643
                if ( controller->isUniqueName() )
                    uniqueNames.erase ( controller->name );
                else
                    shouldReset = true;

                joysticksByName.erase ( controller->getName() );
                joysticks.erase ( id );

                // LOG ( "joysticks :%s", joysticks.empty() ? " (empty)" : "" );
                // for ( const auto& kv : joysticks )
                //     LOG ( "%d -> %08x", kv.first, kv.second.get() );
                break;
            }

            default:
                LOG ( "Unknown event type (%d)", events[i].type );
                break;
        }
    }

    if ( mapInputState )
        for ( auto& kv : joysticks )
            kv.second->state = mapInputState ( kv.second->state, false );
}

void ControllerManager::mappingsChanged ( Controller *controller )
{
    LOG_CONTROLLER ( controller, "mappingsChanged" );

    mappings.mappings[controller->getName()] = controller->getMappings();
    mappings.invalidate();
}

void ControllerManager::clear()
{
    LOG ( "Clearing controllers" );

    joysticks.clear();
    joysticksByName.clear();
}

static bool compareControllerName ( const Controller *a, const Controller *b )
{
    return a->getName() < b->getName();
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

vector<const Controller *> ControllerManager::getJoysticks() const
{
    vector<const Controller *> controllers;
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

vector<const Controller *> ControllerManager::getControllers() const
{
    vector<const Controller *> controllers;
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
        THROW_SDL_EXCEPTION ( SDL_GetError(), "SDL_INIT_JOYSTICK failed", ERROR_CONTROLLER_INIT );

    initialized = true;

    mapInputState = 0;
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

void ControllerMappings::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( mappings.size() );

    for ( const auto& kv : mappings )
        ar ( kv.first, Protocol::encode ( kv.second ) );
}

void ControllerMappings::load ( cereal::BinaryInputArchive& ar )
{
    size_t count;
    ar ( count );

    string name;
    string buffer;
    size_t consumed;

    for ( size_t i = 0; i < count; ++i )
    {
        ar ( name, buffer );

        mappings[name] = Protocol::decode ( &buffer[0], buffer.size(), consumed );

        ASSERT ( consumed == buffer.size() );
    }
}
