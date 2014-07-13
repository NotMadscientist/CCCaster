#pragma once

#include "IndexedGuid.h"

#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "CONTROLLER=%08x; state=%08x; guid='%s'; " FORMAT,                                                    \
          CONTROLLER, CONTROLLER->state, CONTROLLER->guid, ## __VA_ARGS__ )

// Forward declarations
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

class Controller
{
    // Enum type for keyboard controller
    enum KeyboardEnum { Keyboard };

    // Controller unique identifier
    IndexedGuid guid;

    // Controller state
    uint32_t state;

    // Construct a keyboard or joystick controller
    Controller ( KeyboardEnum );
    Controller ( SDL_Joystick *stick );

public:

    friend class ControllerManager;
};
