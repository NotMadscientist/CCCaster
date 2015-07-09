#include "Controller.hpp"
#include "ControllerManager.hpp"
#include "Constants.hpp"
#include "Exceptions.hpp"
#include "ErrorStrings.hpp"

#include <windows.h>

#include <cstdlib>
#include <limits>

using namespace std;


static string getVKeyName ( uint32_t vkCode, uint32_t scanCode, bool isExtended )
{
    switch ( vkCode )
    {
#include "KeyboardVKeyNames.hpp"

        default:
            break;
    }

    if ( isExtended )
        scanCode |= 0x100;

    char name[4096];

    if ( GetKeyNameText ( scanCode << 16, name, sizeof ( name ) ) > 0 )
        return name;
    else
        return format ( "Key Code 0x%02X", vkCode );
}

static inline char getAxisSign ( uint8_t value )
{
    if ( value == AXIS_POSITIVE )
        return '+';

    if ( value == AXIS_NEGATIVE )
        return '-';

    return '0';
}

static inline string getHatString ( uint8_t value )
{
    switch ( value )
    {
        case 8:
            return "Up";
        case 9:
            return "Up-right";
        case 6:
            return "Right";
        case 3:
            return "Down-right";
        case 2:
            return "Down";
        case 1:
            return "Down-left";
        case 4:
            return "Left";
        case 7:
            return "Up-left";
        case 5:
            return "Centered";

        default:
            break;
    }

    return "Unknown";
}


void Controller::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
    // Ignore keyboard events for joystick (except ESC)
    if ( isJoystick() && vkCode != VK_ESCAPE )
        return;

    // Only use keyboard down events for mapping
    if ( isKeyboard() && ! ( isDown && mapping.key ) )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        doClearMapping ( mapping.key );

        const string name = getVKeyName ( vkCode, scanCode, isExtended );

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( mapping.key & ( 1u << i ) )
            {
                keyboardMappings.codes[i] = vkCode;
                keyboardMappings.names[i] = name;
            }
            else if ( keyboardMappings.codes[i] == vkCode )
            {
                keyboardMappings.codes[i] = 0;
                keyboardMappings.names[i].clear();
            }
        }

        keyboardMappings.invalidate();
        key = mapping.key;

        LOG_CONTROLLER ( this, "Mapped key [0x%02X] %s to %08x ", vkCode, name, mapping.key );
    }

    if ( ! ( mapping.options & MAP_CONTINUOUSLY ) )
        cancelMapping();

    if ( key )
        ControllerManager::get().mappingsChanged ( this );

    if ( owner )
        owner->controllerKeyMapped ( this, key );
}

void Controller::joystickAxisEvent ( uint8_t axis, uint8_t value )
{
    if ( joystick.state.isNeutral() && ( mapping.options & MAP_WAIT_NEUTRAL ) )
        mapping.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t mask = joystickMappings.axes[axis][AXIS_CENTERED];

    if ( mapping.key != 0
            && ! ( mapping.options & MAP_WAIT_NEUTRAL )
            && ! ( ( mapping.options & MAP_PRESERVE_DIRS ) && ( mask & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = mapping.active.axes[axis];

        // Done mapping if the axis returned to neutral
        if ( value == AXIS_CENTERED && activeValue != AXIS_CENTERED )
        {
            doClearMapping ( mapping.key );

            joystickMappings.axes[axis][activeValue] = mapping.key;

            // Set bit mask for neutral value
            joystickMappings.axes[axis][AXIS_CENTERED] =
                ( joystickMappings.axes[axis][AXIS_POSITIVE] |
                  joystickMappings.axes[axis][AXIS_NEGATIVE] );

            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped axis%u value=%c to %08x", axis, getAxisSign ( activeValue ), mapping.key );

            Owner *owner = this->owner;
            const uint32_t key = mapping.key;

            if ( mapping.options & MAP_CONTINUOUSLY )
                mapping.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != AXIS_CENTERED )
            return;

        mapping.active.axes[axis] = value;
        return;
    }

    if ( ! mask )
        return;

    const uint32_t keyMapped = joystickMappings.axes[axis][value];

    state &= ~mask;

    if ( value != AXIS_CENTERED )
        state |= keyMapped;

    LOG_CONTROLLER ( this, "value=%c", getAxisSign ( value ) );
}

void Controller::joystickHatEvent ( uint8_t hat, uint8_t value )
{
    if ( joystick.state.isNeutral() && ( mapping.options & MAP_WAIT_NEUTRAL ) )
        mapping.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t mask = joystickMappings.hats[hat][5];

    if ( mapping.key != 0
            && ( value % 2 == 0 || value == 5 ) // Only map up/down/left/right, and finish on neutral
            && ! ( mapping.options & MAP_WAIT_NEUTRAL )
            && ! ( ( mapping.options & MAP_PRESERVE_DIRS ) && ( mask & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = mapping.active.hats[hat];

        // Done mapping if the hat is centered again
        if ( value == 5 && activeValue != 5 )
        {
            doClearMapping ( mapping.key );

            joystickMappings.hats[hat][activeValue] = mapping.key;

            // Set bit mask for neutral value
            joystickMappings.hats[hat][5] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][4] |
                                              joystickMappings.hats[hat][6] | joystickMappings.hats[hat][8] );

            // Set bit masks for diagonal values
            joystickMappings.hats[hat][1] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][4] );
            joystickMappings.hats[hat][3] = ( joystickMappings.hats[hat][2] | joystickMappings.hats[hat][6] );
            joystickMappings.hats[hat][7] = ( joystickMappings.hats[hat][8] | joystickMappings.hats[hat][4] );
            joystickMappings.hats[hat][9] = ( joystickMappings.hats[hat][8] | joystickMappings.hats[hat][6] );

            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped hat%u value=%d to %08x", hat, activeValue, mapping.key );

            Owner *owner = this->owner;
            const uint32_t key = mapping.key;

            if ( mapping.options & MAP_CONTINUOUSLY )
                mapping.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != 5 )
            return;

        mapping.active.hats[hat] = value;
        return;
    }

    if ( ! mask )
        return;

    const uint32_t keyMapped = joystickMappings.hats[hat][value];

    state &= ~mask;

    if ( value != 5 )
        state |= keyMapped;

    LOG_CONTROLLER ( this, "value=%u", value );
}

void Controller::joystickButtonEvent ( uint8_t button, uint8_t value )
{
    if ( joystick.state.isNeutral() && ( mapping.options & MAP_WAIT_NEUTRAL ) )
        mapping.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t keyMapped = joystickMappings.buttons[button];

    if ( mapping.key != 0
            && ! ( mapping.options & MAP_WAIT_NEUTRAL )
            && ! ( ( mapping.options & MAP_PRESERVE_DIRS ) && ( keyMapped & MASK_DIRS ) ) )
    {
        const bool isActive = ( mapping.active.buttons & ( 1u << button ) );

        // Done mapping if the button was released
        if ( !value && isActive )
        {
            doClearMapping ( mapping.key );

            joystickMappings.buttons[button] = mapping.key;
            joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", button, mapping.key );

            Owner *owner = this->owner;
            const uint32_t key = mapping.key;

            if ( mapping.options & MAP_CONTINUOUSLY )
                mapping.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick buttons
        if ( isActive )
            return;

        mapping.active.buttons |= ( 1u << button );
        return;
    }

    if ( keyMapped == 0 )
        return;

    if ( value )
        state |= keyMapped;
    else
        state &= ~keyMapped;

    LOG_CONTROLLER ( this, "button=%d; value=%d", button, value );
}

static unordered_set<string> namesWithIndex;

static unordered_map<string, uint32_t> origNameCount;

static string nextName ( const string& name )
{
    if ( namesWithIndex.find ( name ) == namesWithIndex.end() )
        return name;

    uint32_t index = 2;

    while ( namesWithIndex.find ( format ( "%s (%d)", name, index ) ) != namesWithIndex.end() )
    {
        if ( index == numeric_limits<uint32_t>::max() )
            THROW_EXCEPTION ( ERROR_TOO_MANY_CONTROLLERS, "", name );

        ++index;
    }

    return format ( "%s (%d)", name, index );
}

bool Controller::isUniqueName() const { return ( origNameCount[name] == 1 ); }

Controller::Controller ( KeyboardEnum ) : name ( "Keyboard" )
{
    keyboardMappings.name = name;
    namesWithIndex.insert ( keyboardMappings.name );
    origNameCount[name] = 1;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New keyboard" );
}

Controller::Controller ( const JoystickInfo& info ) : name ( info.name ), joystick ( info )
{
    joystickMappings.name = nextName ( name );
    namesWithIndex.insert ( joystickMappings.name );

    const auto it = origNameCount.find ( name );
    if ( it == origNameCount.end() )
        origNameCount[name] = 1;
    else
        ++it->second;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New joystick: %s; %u axe(s); %u hat(s); %u button(s)",
                     name, joystick.info.numAxes, joystick.info.numHats, joystick.info.numButtons );
}

Controller::~Controller()
{
    LOG_CONTROLLER ( this, "Deleting controller" );

    namesWithIndex.erase ( getName() );

    uint32_t& count = origNameCount[name];

    if ( count > 1 )
        --count;
    else
        origNameCount.erase ( name );

    KeyboardManager::get().unhook();
}

string Controller::getMapping ( uint32_t key, const string& placeholder ) const
{
    if ( key == mapping.key && !placeholder.empty() )
        return placeholder;

    if ( isKeyboard() )
    {
        for ( uint8_t i = 0; i < 32; ++i )
            if ( ( key & ( 1u << i ) ) && keyboardMappings.codes[i] )
                return keyboardMappings.names[i];

        return "";
    }
    else if ( isJoystick() )
    {
        string mapping;

        for ( uint8_t axis = 0; axis < joystick.info.numAxes; ++axis )
        {
            for ( uint8_t value = AXIS_POSITIVE; value <= AXIS_NEGATIVE; ++value )
            {
                if ( joystickMappings.axes[axis][value] != key )
                    continue;

                const string str = format ( "%c %s", getAxisSign ( value ), joystick.info.axisNames[axis] );

                if ( mapping.empty() )
                    mapping = str;
                else
                    mapping += ", " + str;
            }
        }

        for ( uint8_t hat = 0; hat < joystick.info.numHats; ++hat )
        {
            for ( uint8_t value = 2; value <= 8; value += 2 )
            {
                if ( joystickMappings.hats[hat][value] != key )
                    continue;

                string str = "DPad " + getHatString ( value );

                if ( hat > 0 )
                    str += format ( " (%u)", hat + 1 );

                if ( mapping.empty() )
                    mapping = str;
                else
                    mapping += ", " + str;
            }
        }

        for ( uint8_t button = 0; button < joystick.info.numButtons; ++button )
        {
            if ( joystickMappings.buttons[button] != key )
                continue;

            const string str = format ( "Button %u", button + 1 );

            if ( mapping.empty() )
                mapping = str;
            else
                mapping += ", " + str;
        }

        return mapping;
    }

    return "";
}

void Controller::setMappings ( const array<char, 10>& config )
{
    ASSERT ( isKeyboard() == true );

    LOG_CONTROLLER ( this, "Raw keyboard mappings" );

    static const array<uint32_t, 10> bits =
    {
        BIT_DOWN,
        BIT_UP,
        BIT_LEFT,
        BIT_RIGHT,
        ( CC_BUTTON_A | CC_BUTTON_CONFIRM ) << 8,
        ( CC_BUTTON_B | CC_BUTTON_CANCEL ) << 8,
        CC_BUTTON_C << 8,
        CC_BUTTON_D << 8,
        CC_BUTTON_E << 8,
        CC_BUTTON_START << 8,
    };

    doClearMapping();

    for ( uint8_t i = 0; i < 10; ++i )
    {
        const uint32_t vkCode = MapVirtualKey ( config[i], MAPVK_VSC_TO_VK_EX );
        const string name = getVKeyName ( vkCode, config[i], false );

        for ( uint8_t j = 0; j < 32; ++j )
        {
            if ( bits[i] & ( 1u << j ) )
            {
                keyboardMappings.codes[j] = vkCode;
                keyboardMappings.names[j] = name;
            }
            else if ( keyboardMappings.codes[j] == vkCode )
            {
                keyboardMappings.codes[j] = 0;
                keyboardMappings.names[j].clear();
            }
        }
    }

    keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const KeyboardMappings& mappings )
{
    ASSERT ( isKeyboard() == true );

    LOG_CONTROLLER ( this, "KeyboardMappings" );

    keyboardMappings = mappings;
    keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const JoystickMappings& mappings )
{
    ASSERT ( isJoystick() == true );

    LOG_CONTROLLER ( this, "JoystickMappings" );

    joystickMappings = mappings;
    joystickMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::startMapping ( Owner *owner, uint32_t key, uint8_t options )
{
    if ( this->mapping.options & MAP_CONTINUOUSLY )
        mapping.active.clear();
    else
        cancelMapping();

    LOG_CONTROLLER ( this, "Starting mapping %08x", key );

    if ( ! joystick.state.isNeutral() )
        options |= MAP_WAIT_NEUTRAL;

    this->owner = owner;
    this->mapping.key = key;
    this->mapping.options = options;
}

void Controller::cancelMapping()
{
    LOG_CONTROLLER ( this, "Cancel mapping %08x", mapping.key );

    owner = 0;
    mapping.key = 0;
    mapping.options = 0;
    mapping.active.clear();
}

void Controller::doClearMapping ( uint32_t keys )
{
    for ( uint8_t i = 0; i < 32; ++i )
    {
        if ( keys & ( 1u << i ) )
        {
            keyboardMappings.codes[i] = 0;
            keyboardMappings.names[i].clear();
            keyboardMappings.invalidate();
        }
    }

    for ( auto& a : joystickMappings.axes )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : joystickMappings.hats )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : joystickMappings.buttons )
    {
        if ( a & keys )
        {
            a = 0;
            joystickMappings.invalidate();
        }
    }
}

void Controller::clearMapping ( uint32_t keys )
{
    doClearMapping ( keys );

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::doResetToDefaults()
{
    if ( ! isJoystick() )
        return;

    // Default axis mappings
    for ( uint8_t axis = 0; axis < joystick.info.numAxes; ++axis )
    {
        if ( joystick.info.axisNames[axis].empty() )
            continue;

        if ( joystick.info.axisNames[axis][0] == 'X' )
        {
            joystickMappings.axes[axis][AXIS_CENTERED] = MASK_X_AXIS;
            joystickMappings.axes[axis][AXIS_POSITIVE] = BIT_RIGHT;
            joystickMappings.axes[axis][AXIS_NEGATIVE] = BIT_LEFT;
        }
        else if ( joystick.info.axisNames[axis][0] == 'Y' )
        {
            joystickMappings.axes[axis][AXIS_CENTERED] = MASK_Y_AXIS;
            joystickMappings.axes[axis][AXIS_POSITIVE] = BIT_UP;
            joystickMappings.axes[axis][AXIS_NEGATIVE] = BIT_DOWN;
        }
    }

    // Default hat mappings
    for ( uint8_t hat = 0; hat < joystick.info.numHats; ++hat )
    {
        joystickMappings.hats[hat][5] = MASK_DIRS;
        joystickMappings.hats[hat][8] = BIT_UP;
        joystickMappings.hats[hat][9] = BIT_UP | BIT_RIGHT;
        joystickMappings.hats[hat][6] = BIT_RIGHT;
        joystickMappings.hats[hat][3] = BIT_DOWN | BIT_RIGHT;
        joystickMappings.hats[hat][2] = BIT_DOWN;
        joystickMappings.hats[hat][1] = BIT_DOWN | BIT_LEFT;
        joystickMappings.hats[hat][4] = BIT_LEFT;
        joystickMappings.hats[hat][7] = BIT_UP | BIT_LEFT;
    }

    // Clear all buttons
    doClearMapping ( MASK_BUTTONS );

    // Default deadzone
    joystickMappings.deadzone = DEFAULT_DEADZONE;

    joystickMappings.invalidate();
}

void Controller::resetToDefaults()
{
    if ( ! isJoystick() )
        return;

    doResetToDefaults();

    ControllerManager::get().mappingsChanged ( this );
}

bool Controller::saveMappings ( const string& file ) const
{
    if ( isKeyboard() )
        return ControllerManager::saveMappings ( file, keyboardMappings );
    else
        return ControllerManager::saveMappings ( file, joystickMappings );
}

bool Controller::loadMappings ( const string& file )
{
    MsgPtr msg = ControllerManager::loadMappings ( file );

    if ( ! msg )
        return false;

    if ( isKeyboard() )
    {
        if ( msg->getMsgType() != MsgType::KeyboardMappings )
        {
            LOG ( "Invalid keyboard mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<KeyboardMappings>().name != keyboardMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != keyboard '%s'",
                  msg->getAs<KeyboardMappings>().name, keyboardMappings.name );
        }

        keyboardMappings = msg->getAs<KeyboardMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }
    else // if ( isJoystick() )
    {
        if ( msg->getMsgType() != MsgType::JoystickMappings )
        {
            LOG ( "Invalid joystick mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<JoystickMappings>().name != joystickMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != joystick '%s'",
                  msg->getAs<JoystickMappings>().name, joystickMappings.name );
        }

        joystickMappings = msg->getAs<JoystickMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }

    return true;
}
