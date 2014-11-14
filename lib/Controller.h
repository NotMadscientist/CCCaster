#pragma once

#include "IndexedGuid.h"
#include "KeyboardManager.h"

#include <unordered_map>
#include <string>


#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "%s: controller=%08x; joystick=%08x; state=%08x; " FORMAT,                                            \
          CONTROLLER->name, CONTROLLER, CONTROLLER->joystick, CONTROLLER->state, ## __VA_ARGS__ )


#define BIT_UP          ( 0x00000001u )
#define BIT_DOWN        ( 0x00000002u )
#define BIT_LEFT        ( 0x00000004u )
#define BIT_RIGHT       ( 0x00000008u )

#define MASK_X_AXIS     ( 0x0000000Cu )
#define MASK_Y_AXIS     ( 0x00000003u )
#define MASK_DIRS       ( 0x0000000Fu )
#define MASK_BUTTONS    ( 0xFFFFFFF0u )


// Forward declarations
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;
struct SDL_JoyAxisEvent;
struct SDL_JoyHatEvent;
struct SDL_JoyButtonEvent;


class Controller : public KeyboardManager::Owner
{
public:

    struct Owner
    {
        virtual void doneMapping ( Controller *controller, uint32_t key ) = 0;
    };

    Owner *owner = 0;

private:

    // Enum type for keyboard controller
    enum KeyboardEnum { Keyboard };

    // Mapping tuple
    struct Mapping { const uint8_t type, index, value; const uint32_t state; };

    // SDL joystick pointer, 0 for keyboard
    SDL_Joystick *joystick = 0;

    // Controller unique identifier
    IndexedGuid guid;

    // Controller state
    uint32_t state = 0;

    // Keyboard mappings: bit index -> virtual key code
    uint32_t keyCode[32];
    std::string keyName[32];

    // Joystick mappings: event type -> axis/hat/button index -> axis/hat/button value -> mapped value
    //
    // The neutral axis/hat/button value is mapped to a bit mask of all the mapped values on the same index.
    //
    // Example:                                         Example:
    //   Axis up      -> 0b0001                           Hat up      -> 0b0001   Hat left  -> 0b0100
    //   Axis down    -> 0b0010                           Hat down    -> 0b0010   Hat right -> 0b1000
    //
    // Then:                                            Then:
    //   Axis neutral -> 0b0011                           Hat neutral -> 0b1111
    //
    uint32_t mappings[3][256][16];

    // The current key to map to an event
    uint32_t keyToMap = 0;

    // The currently active mappings for the above key
    uint32_t activeMappings[3][256][16];

    // Guid mapped to a bitset of indices, used to determine a free index for a duplicate guid
    static std::unordered_map<Guid, uint32_t> guidBitset;

    // Keyboard event callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown );

    // Joystick event callbacks
    void joystickEvent ( const SDL_JoyAxisEvent& event );
    void joystickEvent ( const SDL_JoyHatEvent& event );
    void joystickEvent ( const SDL_JoyButtonEvent& event );

    // Construct a keyboard / joystick controller
    Controller ( KeyboardEnum );
    Controller ( SDL_Joystick *joystick );

public:

    // Controller name
    const std::string name;

    // Joystick axis deadzones
    uint16_t deadzones[256];

    // Basic destructor
    ~Controller();

    // Get the mapping for the given key as a human-readable string
    std::string getMapping ( uint32_t key ) const;

    // Start / cancel mapping for the given key
    void startMapping ( Owner *owner, uint32_t key, const void *window = 0 );
    void cancelMapping();

    // Clear this controller's mapping(s)
    void clearMapping ( uint32_t keys = 0xFFFFFFFF );

    // Get the controller state
    uint32_t getState() const { return state; }

    // Indicates if this is a keyboard / joystick controller
    bool isKeyboard() const { return ( joystick == 0 ); }
    bool isJoystick() const { return ( joystick != 0 ); }

    // Indicates if this is the only joystick with its guid
    bool isOnlyGuid() const;

    friend class ControllerManager;
};
