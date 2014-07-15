#pragma once

#include "IndexedGuid.h"

#include <unordered_map>

#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "controller=%08x; joystick=%08x; state=%08x; " FORMAT,                                                \
          CONTROLLER, CONTROLLER->joystick, CONTROLLER->state, ## __VA_ARGS__ )

// Forward declarations
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;
struct SDL_JoyAxisEvent;
struct SDL_JoyHatEvent;
struct SDL_JoyButtonEvent;

class Controller
{
    // Enum type for keyboard controller
    enum KeyboardEnum { Keyboard };

    // SDL joystick pointer, 0 for keyboard
    SDL_Joystick *joystick;

    // Controller unique identifier
    IndexedGuid guid;

    // Controller state
    uint32_t state;

    // Guid mapped to a bitset of indicies
    static std::unordered_map<Guid, uint32_t> guidBitset;

    // Joystick event callbacks
    void joystickEvent ( const SDL_JoyAxisEvent& event );
    void joystickEvent ( const SDL_JoyHatEvent& event );
    void joystickEvent ( const SDL_JoyButtonEvent& event );

    // Construct a keyboard or joystick controller
    Controller ( KeyboardEnum );
    Controller ( SDL_Joystick *joystick );

public:

    // Basic destructor
    ~Controller();

    friend class ControllerManager;
};
