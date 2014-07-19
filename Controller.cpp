#include "Controller.h"
#include "Logger.h"

#include <SDL.h>

#include <cstdlib>

using namespace std;


#define DEFAULT_DEADZONE 25000


void Controller::joystickEvent ( const SDL_JoyAxisEvent& event )
{
    if ( event.axis > 3 )
    {
        // TODO handle me
        return;
    }

    if ( event.axis % 2 == 0 ) // X-axis
    {
        state &= ~MASK_X_AXIS;
        if ( abs ( event.value ) > DEFAULT_DEADZONE ) // TODO config per axis
            state |= ( event.value > 0 ? BIT_RIGHT : BIT_LEFT );
    }
    else if ( event.axis % 2 == 1 ) // Y-axis
    {
        state &= ~MASK_Y_AXIS;
        if ( abs ( event.value ) > DEFAULT_DEADZONE ) // TODO config per axis
            state |= ( event.value > 0 ? BIT_DOWN : BIT_UP ); // Y-axis is inverted
    }
}

void Controller::joystickEvent ( const SDL_JoyHatEvent& event )
{
    if ( event.hat == 0 )
    {
        state &= ~MASK_DIRS;
        if ( event.hat != SDL_HAT_CENTERED )
        {
            if ( event.hat & SDL_HAT_UP )
                state |= BIT_UP;
            else if ( event.hat & SDL_HAT_DOWN )
                state |= BIT_DOWN;

            if ( event.hat & SDL_HAT_LEFT )
                state |= BIT_LEFT;
            else if ( event.hat & SDL_HAT_RIGHT )
                state |= BIT_RIGHT;
        }
        return;
    }

    // TODO handle me
}

void Controller::joystickEvent ( const SDL_JoyButtonEvent& event )
{
}

Controller::Controller ( KeyboardEnum ) : joystick ( 0 ), state ( 0 )
{
    memset ( &guid, 0, sizeof ( guid ) );
}

Controller::Controller ( SDL_Joystick *joystick ) : joystick ( joystick ), state ( 0 )
{
    SDL_JoystickGUID guid = SDL_JoystickGetGUID ( joystick );
    memcpy ( &this->guid.guid, guid.data, sizeof ( guid.data ) );

    auto it = guidBitset.find ( this->guid.guid );

    if ( it == guidBitset.end() )
    {
        this->guid.index = 0;
        guidBitset[this->guid.guid] = 1u;
    }
    else
    {
        for ( uint32_t i = 0; i < 32; ++i )
        {
            if ( ( it->second & ( 1u << i ) ) == 0u )
            {
                guidBitset[this->guid.guid] |= ( 1u << i );
                this->guid.index = i;
                return;
            }
        }

        Exception err = toString ( "Too many duplicate guids for: '%s'", this->guid.guid );
        LOG_AND_THROW ( err, "" );
    }
}

Controller::~Controller()
{
    auto it = guidBitset.find ( guid.guid );

    if ( it == guidBitset.end() )
        return;

    if ( guid.index >= 32 )
        return;

    guidBitset[guid.guid] &= ~ ( 1u << guid.index );
}

unordered_map<Guid, uint32_t> Controller::guidBitset;
