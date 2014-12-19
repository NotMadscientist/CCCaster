#pragma once

#include "KeyboardManager.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <array>
#include <algorithm>


#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "%s: controller=%08x; device=%08x; state=%08x; " FORMAT,                                              \
          CONTROLLER->getName(), CONTROLLER, CONTROLLER->joystick.device, CONTROLLER->state, ## __VA_ARGS__ )


#define BIT_UP              ( 0x00000001u )
#define BIT_DOWN            ( 0x00000002u )
#define BIT_LEFT            ( 0x00000004u )
#define BIT_RIGHT           ( 0x00000008u )

#define MASK_X_AXIS         ( 0x0000000Cu )
#define MASK_Y_AXIS         ( 0x00000003u )
#define MASK_DIRS           ( 0x0000000Fu )
#define MASK_BUTTONS        ( 0xFFFFFFF0u )

#define DEFAULT_DEADZONE    ( 0.3f )

#define MAP_PRESERVE_DIRS   ( 0x01u )
#define MAP_CONTINUOUSLY    ( 0x02u )
#define MAP_WAIT_NEUTRAL    ( 0x04u )

#define AXIS_CENTERED       ( 0u )
#define AXIS_POSITIVE       ( 1u )
#define AXIS_NEGATIVE       ( 2u )

#define MAX_NUM_AXES        ( 32u )
#define MAX_NUM_HATS        ( 32u )
#define MAX_NUM_BUTTONS     ( 128u )


struct KeyboardMappings : public SerializableSequence
{
    // Controller unique identifier
    std::string name;

    // bit index -> virtual key code
    uint32_t codes[32];

    // bit index -> key name
    std::string names[32];

    PROTOCOL_MESSAGE_BOILERPLATE ( KeyboardMappings, name, codes, names )
};


struct JoystickMappings : public SerializableSequence
{
    // Controller unique identifier
    std::string name;

    // axis index -> axis value -> mapped key
    //
    // The zero value is mapped to a bit mask of all the mapped values on the same axis index.
    //
    // Example:
    //   Axis up      -> 0b0001
    //   Axis down    -> 0b0010
    //
    // Then:
    //   Axis neutral -> 0b0011
    //
    uint32_t axes[MAX_NUM_AXES][3];

    // hat index -> hat value -> mapped key
    //
    // Hat values correspond to numpad notation, so 0 is unused
    //
    // The 5 value is mapped to a bit mask of all the mapped values on the same hat index.
    //
    // Example:
    //   Hat up      -> 0b0001   Hat left  -> 0b0100
    //   Hat down    -> 0b0010   Hat right -> 0b1000
    //
    // Then:
    //   Hat neutral -> 0b1111
    //
    // Similarly, the 1, 3, 7, 9 values should be mapped to the correct bit masks as well.
    //
    uint32_t hats[MAX_NUM_HATS][10];

    // button index -> mapped key
    uint32_t buttons[MAX_NUM_BUTTONS];

    // Axis deadzone
    float deadzone = DEFAULT_DEADZONE;

    PROTOCOL_MESSAGE_BOILERPLATE ( JoystickMappings, name, axes, buttons, deadzone )
};


struct JoystickState
{
    uint8_t axes[MAX_NUM_AXES];
    uint8_t hats[MAX_NUM_HATS];
    uint8_t buttons = 0;

    JoystickState() { clear(); }

    bool isNeutral() const
    {
        for ( auto& a : axes )
        {
            if ( a != AXIS_CENTERED )
                return false;
        }
        for ( auto& a : hats )
        {
            if ( a != 5 )
                return false;
        }
        return ( buttons == 0 );
    }

    void clear()
    {
        for ( auto& a : axes )
            a = AXIS_CENTERED;
        for ( auto& a : hats )
            a = 5;
        buttons = 0;
    }
};


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

    // Original controller name
    const std::string name;

    // Controller states
    uint32_t prevState = 0, state = 0;

    // Keyboard mappings
    KeyboardMappings keyboardMappings;

    // Joystick mappings
    JoystickMappings joystickMappings;

    struct JoystickInternalState
    {
        // Implementation specific device pointer, 0 for keyboard
        void *const device = 0;

        // Joystick capabilities
        const uint32_t numAxes = 0, numHats = 0, numButtons = 0;

        // Joystick states
        JoystickState prevState, state;

        JoystickInternalState() {}
        JoystickInternalState ( void *device, uint32_t numAxes, uint32_t numHats, uint32_t numButtons )
            : device ( device )
            , numAxes ( std::min ( numAxes, MAX_NUM_AXES ) )
            , numHats ( std::min ( numHats, MAX_NUM_HATS ) )
            , numButtons ( std::min ( numButtons, MAX_NUM_BUTTONS ) ) {}
    };

    JoystickInternalState joystick;

    struct MappingInternalState
    {
        // The current key to map to an event
        uint32_t key = 0;

        // Mappings options
        uint8_t options = 0;

        // The current active joystick state
        JoystickState active;
    };

    MappingInternalState mapping;

    // Keyboard event callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown );

    // Joystick event callbacks
    void joystickAxisEvent ( uint8_t axis, uint8_t value );
    void joystickHatEvent ( uint8_t hat, uint8_t value );
    void joystickButtonEvent ( uint8_t button, bool isDown );

    // Construct a keyboard / joystick controller
    Controller ( KeyboardEnum );
    Controller ( const std::string& name, void *device, uint32_t numAxes, uint32_t numHats, uint32_t numButtons );

    // Clear this controller's mapping(s) without callback to ControllerManager
    void doClearMapping ( uint32_t keys = 0xFFFFFFFF );

    // Reset this joystick's mapping(s) without callback to ControllerManager
    void doResetToDefaults();

public:

    // Basic destructor
    ~Controller();

    // Get controller name with index
    const std::string& getName() const
    {
        if ( isKeyboard() )
            return keyboardMappings.name;
        else
            return joystickMappings.name;
    }

    // Get the original name of the controller
    const std::string& getOrigName() const { return name; }

    // Indicates if this is the only controller with its original name
    bool isUniqueName() const;

    // Get the mapping for the given key as a human-readable string
    std::string getMapping ( uint32_t key, const std::string& placeholder = "" ) const;

    // Get the mappings for this controller
    MsgPtr getMappings() const
    {
        if ( isKeyboard() )
            return keyboardMappings.clone();
        else
            return joystickMappings.clone();
    }

    // Set the mappings for this controller
    void setMappings ( const std::array<char, 10>& config );
    void setMappings ( const KeyboardMappings& mappings );
    void setMappings ( const JoystickMappings& mappings );

    // Start / cancel mapping for the given key
    void startMapping ( Owner *owner, uint32_t key,
                        const void *window = 0,                             // Window to match for keyboard events
                        const std::unordered_set<uint32_t>& ignore = {},    // VK codes to IGNORE
                        uint8_t options = 0 );                              // Mapping options
    void cancelMapping();
    bool isMapping() const { return ( mapping.key != 0 ); }

    // Clear this controller's mapping(s)
    void clearMapping ( uint32_t keys );
    void clearAllMappings() { clearMapping ( 0xFFFFFFFF ); }

    // Reset default mappings (joystick only)
    void resetToDefaults();

    // Get / set joystick deadzone
    float getDeadzone() { return joystickMappings.deadzone; }
    void setDeadzone ( float deadzone ) { joystickMappings.deadzone = deadzone; joystickMappings.invalidate(); }

    // Get the joystick any-button state
    bool getPrevAnyButton() const { return joystick.prevState.buttons; }
    bool getAnyButton() const { return joystick.state.buttons; }

    // Get the controller state
    uint32_t getPrevState() const { return prevState; }
    uint32_t getState() const { return state; }

    // Indicates if this is a keyboard / joystick controller
    bool isKeyboard() const { return ( joystick.device == 0 ); }
    bool isJoystick() const { return ( joystick.device != 0 ); }

    // Save / load mappings for this controller
    bool saveMappings ( const std::string& file ) const;
    bool loadMappings ( const std::string& file );

    friend class ControllerManager;
};
