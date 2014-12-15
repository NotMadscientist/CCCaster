#pragma once

#include "KeyboardManager.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <array>


#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "%s: controller=%08x; joystick=%08x; state=%08x; " FORMAT,                                            \
          CONTROLLER->getName(), CONTROLLER, CONTROLLER->joystick, CONTROLLER->state, ## __VA_ARGS__ )


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

#define AXIS_CENTERED       ( 0 )
#define AXIS_POSITIVE       ( 1 )
#define AXIS_NEGATIVE       ( 2 )

#define MAX_NUM_AXES        ( 32 )
#define MAX_NUM_BUTTONS     ( 128 )


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

    // button index -> mapped key
    uint32_t buttons[MAX_NUM_BUTTONS];

    // Axis deadzone
    float deadzone = DEFAULT_DEADZONE;

    PROTOCOL_MESSAGE_BOILERPLATE ( JoystickMappings, name, axes, buttons, deadzone )
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

    // Implementation specific joystick pointer, 0 for keyboard
    void *joystick = 0;

    // Joystick any-button state
    uint8_t prevAnyButton = 0, anyButton = 0;

    // Controller state
    uint32_t prevState = 0, state = 0;

    // Keyboard mappings
    KeyboardMappings keybd;

    // Joystick mappings
    JoystickMappings stick;

    // The current key to map to an event
    uint32_t keyToMap = 0;

    // Flag to wait for neutral state before mapping
    bool waitForNeutral = false;

    // Mappings options
    uint8_t options = 0;

    // The currently active joystick mappings for the above key
    JoystickMappings active;

    // Keyboard event callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown );

    // Joystick event callbacks
    void joystickAxisEvent ( uint8_t axis, uint8_t value );
    void joystickButtonEvent ( uint8_t button, bool isDown );

    // Construct a keyboard / joystick controller
    Controller ( KeyboardEnum );
    Controller ( const std::string& name, void *joystick );

    // Clear this controller's mapping(s) without callback to ControllerManager
    void doClearMapping ( uint32_t keys = 0xFFFFFFFF );

    // Reset this joystick's mapping(s) without callback to ControllerManager
    void doResetToDefaults();

    // Clear active joytstick mappings
    void clearActive();

public:

    // Basic destructor
    ~Controller();

    // Get controller name with index
    const std::string& getName() const
    {
        if ( isKeyboard() )
            return keybd.name;
        else
            return stick.name;
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
            return keybd.clone();
        else
            return stick.clone();
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
    bool isMapping() const { return ( keyToMap != 0 ); }

    // Clear this controller's mapping(s)
    void clearMapping ( uint32_t keys );
    void clearAllMappings() { clearMapping ( 0xFFFFFFFF ); }

    // Reset default mappings (joystick only)
    void resetToDefaults();

    // Get / set joystick deadzone
    float getDeadzone() { return stick.deadzone; }
    void setDeadzone ( float deadzone ) { stick.deadzone = deadzone; stick.invalidate(); }

    // Get the joystick any-button state
    bool getPrevAnyButton() const { return prevAnyButton; }
    bool getAnyButton() const { return anyButton; }

    // Get the controller state
    uint32_t getPrevState() const { return prevState; }
    uint32_t getState() const { return state; }

    // Indicates if this is a keyboard / joystick controller
    bool isKeyboard() const { return ( joystick == 0 ); }
    bool isJoystick() const { return ( joystick != 0 ); }

    // Save / load mappings for this controller
    bool saveMappings ( const std::string& file ) const;
    bool loadMappings ( const std::string& file );

    friend class ControllerManager;
};
