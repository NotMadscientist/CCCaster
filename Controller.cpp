#include "Controller.h"

#include <SDL.h>

Controller::Controller ( KeyboardEnum ) : state ( 0 )
{
    memset ( &guid, 0, sizeof ( guid ) );
}

Controller::Controller ( SDL_Joystick *stick ) : state ( 0 )
{
    SDL_JoystickGUID guid = SDL_JoystickGetGUID ( stick );
    memcpy ( this->guid.guid, guid.data, sizeof ( guid.data ) );
    this->guid.index = 0;
}
