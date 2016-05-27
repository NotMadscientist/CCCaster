#pragma once

#include "KeyboardManager.hpp"
#include "Algorithms.hpp"
#include "Thread.hpp"
#include "Guid.hpp"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <array>


#define LOG_CONTROLLER(CONTROLLER, FORMAT, ...)                                                                 \
    LOG ( "%s: controller=%08x; state=%08x; " FORMAT,                                                           \
          CONTROLLER->getName(), CONTROLLER, CONTROLLER->_state, ## __VA_ARGS__ )


#define BIT_UP              ( 0x00000001u )
#define BIT_DOWN            ( 0x00000002u )
#define BIT_LEFT            ( 0x00000004u )
#define BIT_RIGHT           ( 0x00000008u )

#define MASK_X_AXIS         ( 0x0000000Cu )
#define MASK_Y_AXIS         ( 0x00000003u )
#define MASK_DIRS           ( 0x0000000Fu )
#define MASK_BUTTONS        ( 0xFFFFFFF0u )

#define MIN_DEADZONE        ( 328 )     // 328   / 32767 ~ 0.01
#define MAX_DEADZONE        ( 32439 )   // 32439 / 32767 ~ 0.99
#define DEFAULT_DEADZONE    ( 25000 )   // 25000 / 32767 ~ 0.76

#define MAP_PRESERVE_DIRS   ( 0x01u )
#define MAP_CONTINUOUSLY    ( 0x02u )
#define MAP_WAIT_NEUTRAL    ( 0x04u )

#define AXIS_CENTERED       ( 0u )
#define AXIS_POSITIVE       ( 1u )
#define AXIS_NEGATIVE       ( 2u )

#define MAX_NUM_AXES        ( 32u )
#define MAX_NUM_HATS        ( 32u )
#define MAX_NUM_BUTTONS     ( 32u )


struct KeyboardMappings : public SerializableSequence
{
    // Controller unique identifier
    std::string name;

    // Bit index -> virtual key code
    uint32_t codes[32] = {{ 0 }};

    // Bit index -> key name
    std::string names[32];

    PROTOCOL_MESSAGE_BOILERPLATE ( KeyboardMappings, name, codes, names )
};


struct JoystickMappings : public SerializableSequence
{
    // Controller unique identifier
    std::string name;

    // Axis index -> axis value -> mapped key
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
    uint32_t axes[MAX_NUM_AXES][3] = {{ 0 }};

    // Hat index -> hat value -> mapped key
    //
    // Hat values correspond to numpad notation, so 0 is unused
    //
    // The 5 value is mapped to a bit mask of all the mapped values on the same hat index.
    //
    // Example:
    //   Hat 8 (up)      -> 0b0001   Hat 4 (left)  -> 0b0100
    //   Hat 2 (down)    -> 0b0010   Hat 6 (right) -> 0b1000
    //
    // Then:
    //   Hat 5 (neutral) -> 0b1111
    //
    // Similarly, the 1, 3, 7, 9 values should be mapped to the correct bit masks as well.
    //
    uint32_t hats[MAX_NUM_HATS][10] = {{ 0 }};

    // Button index -> mapped key
    uint32_t buttons[MAX_NUM_BUTTONS] = {{ 0 }};

    // Axis deadzone range (0,32767)
    uint32_t deadzone = DEFAULT_DEADZONE;

    PROTOCOL_MESSAGE_BOILERPLATE ( JoystickMappings, name, axes, hats, buttons, deadzone )
};


struct JoystickState
{
    uint8_t axes[MAX_NUM_AXES];
    uint8_t hats[MAX_NUM_HATS];
    uint32_t buttons = 0;

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
        return true;
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


struct JoystickInfo
{
    // Implementation specific device pointer, 0 for keyboard
    void *device = 0;

    // Joystick friendly name
    std::string name;

    // Joystick product guid
    Guid guid;

    // Joystick capabilities
    uint8_t numAxes = 0, numHats = 0, numButtons = 0;

    // Axis names
    std::vector<std::string> axisNames;

    // Bit mask of axes in use, NOTE implementation specific usage
    uint8_t axisMask = 0;

    JoystickInfo() {}
    JoystickInfo ( void *device ) : device ( device ) {}
};


class Controller : public KeyboardManager::Owner
{
public:

    struct Owner
    {
        virtual void controllerKeyMapped ( Controller *controller, uint32_t key ) = 0;
    };

    Owner *owner = 0;

    // Basic destructor
    ~Controller();

    // Get controller name with index
    const std::string& getName() const
    {
        if ( isKeyboard() )
            return _keyboardMappings.name;
        else
            return _joystickMappings.name;
    }

    // Indicates if this is the only controller with its original name
    bool isUniqueName() const;

    // Get the mapping for the given key as a human-readable string
    std::string getMapping ( uint32_t key, const std::string& placeholder = "" ) const;

    // Get the mappings for this controller
    MsgPtr getMappings() const
    {
        if ( isKeyboard() )
            return _keyboardMappings.clone();
        else
            return _joystickMappings.clone();
    }

    // Set the mappings for this controller
    void setMappings ( const std::array<char, 10>& config );
    void setMappings ( const KeyboardMappings& mappings );
    void setMappings ( const JoystickMappings& mappings );

    // Start / cancel mapping for the given key
    void startMapping ( Owner *owner, uint32_t key, uint8_t options = 0 );
    void cancelMapping();
    bool isMapping() const { return ( _toMap.key != 0 ); }

    // Clear this controller's mapping(s)
    void clearMapping ( uint32_t keys );
    void clearAllMappings() { clearMapping ( 0xFFFFFFFF ); }

    // Reset default mappings (joystick only)
    void resetToDefaults();

    // Get / set joystick deadzone
    float getDeadzone() { return _joystickMappings.deadzone / 32767.0f; }
    void setDeadzone ( float deadzone )
    {
        _joystickMappings.deadzone = ( uint32_t ) clamped<float> ( deadzone * 32767, MIN_DEADZONE, MAX_DEADZONE );
        _joystickMappings.invalidate();
    }

    // Get the controller state
    uint32_t getPrevState() const { return _prevState; }
    uint32_t getState() const { return _state; }

    // Indicates if this is a keyboard / joystick controller
    bool isKeyboard() const { return ( _joystick.info.device == 0 ); }
    bool isJoystick() const { return ( _joystick.info.device != 0 ); }

    // Save / load mappings for this controller
    bool saveMappings ( const std::string& file ) const;
    bool loadMappings ( const std::string& file );

    // Get the raw joystick state
    const JoystickState& getJoystickState() const { return _joystick.state; }

    friend class ControllerManager;
    friend class DllControllerManager;

private:

    // Enum type for keyboard controller
    enum KeyboardEnum { Keyboard };

    // Original controller name
    const std::string _origName;

    // Controller states
    uint32_t _prevState = 0, _state = 0;

    // Keyboard mappings
    KeyboardMappings _keyboardMappings;

    // Joystick mappings
    JoystickMappings _joystickMappings;

    struct JoystickInternalState
    {
        // Joystick information
        const JoystickInfo info;

        // Joystick states
        JoystickState prevState, state;

        JoystickInternalState() {}
        JoystickInternalState ( const JoystickInfo& info ) : info ( info ) {}
    };

    JoystickInternalState _joystick;

    struct MappingInternalState
    {
        // The current key to map to an event
        uint32_t key = 0;

        // Mappings options
        uint8_t options = 0;

        // The current active joystick state
        JoystickState active;
    };

    MappingInternalState _toMap;

    // Keyboard event callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown );

    // Joystick event callbacks
    void joystickAxisEvent ( uint8_t axis, uint8_t value );
    void joystickHatEvent ( uint8_t hat, uint8_t value );
    void joystickButtonEvent ( uint8_t button, uint8_t value );

    // Private constructor, instances should only be created by ControllerManager
    Controller();
    Controller ( const Controller& );
    const Controller& operator= ( const Controller& );

    // Construct a keyboard / joystick controller
    Controller ( KeyboardEnum );
    Controller ( const JoystickInfo& info );

    // Clear this controller's mapping(s) without callback to ControllerManager
    void doClearMapping ( uint32_t keys = 0xFFFFFFFF );

    // Reset this joystick's mapping(s) without callback to ControllerManager
    void doResetToDefaults();
};
