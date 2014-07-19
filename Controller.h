#pragma once

#include "IndexedGuid.h"

#include <unordered_map>


#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "controller=%08x; joystick=%08x; state=%08x; " FORMAT,                                                \
          CONTROLLER, CONTROLLER->joystick, CONTROLLER->state, ## __VA_ARGS__ )


#define BIT_UP          0x00000001u
#define BIT_DOWN        0x00000002u
#define BIT_LEFT        0x00000004u
#define BIT_RIGHT       0x00000008u

#define MASK_X_AXIS     0x0000000Cu
#define MASK_Y_AXIS     0x00000003u
#define MASK_DIRS       0x0000000Fu
#define MASK_BUTTONS    0xFFFFFFF0u


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

    // Guid mapped to a bitset of indicies, used to determine a free index for a duplicate guid
    static std::unordered_map<Guid, uint32_t> guidBitset;

    // Joystick event callbacks
    void joystickEvent ( const SDL_JoyAxisEvent& event );
    void joystickEvent ( const SDL_JoyHatEvent& event );
    void joystickEvent ( const SDL_JoyButtonEvent& event );

    // Construct a keyboard / joystick controller
    Controller ( KeyboardEnum );
    Controller ( SDL_Joystick *joystick );

public:

    // Basic destructor
    ~Controller();

    // Get the controller state
    inline uint32_t getState() const { return state; }

    // Indicates if this is a keyboard / joystick controller
    inline bool isKeyboard() const { return ( joystick == 0 ); }
    inline bool isJoystick() const { return ( joystick != 0 ); }

    friend class ControllerManager;
};
