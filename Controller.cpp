#include "Controller.h"
#include "Logger.h"

#include <SDL.h>

using namespace std;

unordered_map<Guid, uint32_t> Controller::guidBitset;

void Controller::joystickEvent ( const SDL_JoyAxisEvent& event )
{
}

void Controller::joystickEvent ( const SDL_JoyHatEvent& event )
{
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
