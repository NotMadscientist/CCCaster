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
    if ( isKeyboard() && ! ( isDown && _toMap.key ) )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        doClearMapping ( _toMap.key );

        const string name = getVKeyName ( vkCode, scanCode, isExtended );

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( _toMap.key & ( 1u << i ) )
            {
                _keyboardMappings.codes[i] = vkCode;
                _keyboardMappings.names[i] = name;
            }
            else if ( _keyboardMappings.codes[i] == vkCode )
            {
                _keyboardMappings.codes[i] = 0;
                _keyboardMappings.names[i].clear();
            }
        }

        _keyboardMappings.invalidate();
        key = _toMap.key;

        LOG_CONTROLLER ( this, "Mapped key [0x%02X] %s to %08x ", vkCode, name, _toMap.key );
    }

    if ( ! ( _toMap.options & MAP_CONTINUOUSLY ) )
        cancelMapping();

    if ( key )
        ControllerManager::get().mappingsChanged ( this );

    if ( owner )
        owner->controllerKeyMapped ( this, key );
}

void Controller::joystickAxisEvent ( uint8_t axis, uint8_t value )
{
    if ( _joystick.state.isNeutral() && ( _toMap.options & MAP_WAIT_NEUTRAL ) )
        _toMap.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t mask = _joystickMappings.axes[axis][AXIS_CENTERED];

    if ( _toMap.key != 0
            && ! ( _toMap.options & MAP_WAIT_NEUTRAL )
            && ! ( ( _toMap.options & MAP_PRESERVE_DIRS ) && ( mask & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = _toMap.active.axes[axis];

        // Done mapping if the axis returned to neutral
        if ( value == AXIS_CENTERED && activeValue != AXIS_CENTERED )
        {
            doClearMapping ( _toMap.key );

            _joystickMappings.axes[axis][activeValue] = _toMap.key;

            // Set bit mask for neutral value
            _joystickMappings.axes[axis][AXIS_CENTERED] =
                ( _joystickMappings.axes[axis][AXIS_POSITIVE] |
                  _joystickMappings.axes[axis][AXIS_NEGATIVE] );

            _joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped axis%u value=%c to %08x", axis, getAxisSign ( activeValue ), _toMap.key );

            Owner *owner = this->owner;
            const uint32_t key = _toMap.key;

            if ( _toMap.options & MAP_CONTINUOUSLY )
                _toMap.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != AXIS_CENTERED )
            return;

        _toMap.active.axes[axis] = value;
        return;
    }

    if ( ! mask )
        return;

    const uint32_t keyMapped = _joystickMappings.axes[axis][value];

    _state &= ~mask;

    if ( value != AXIS_CENTERED )
        _state |= keyMapped;

    LOG_CONTROLLER ( this, "value=%c", getAxisSign ( value ) );
}

void Controller::joystickHatEvent ( uint8_t hat, uint8_t value )
{
    if ( _joystick.state.isNeutral() && ( _toMap.options & MAP_WAIT_NEUTRAL ) )
        _toMap.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t mask = _joystickMappings.hats[hat][5];

    if ( _toMap.key != 0
            && ( value % 2 == 0 || value == 5 ) // Only map up/down/left/right, and finish on neutral
            && ! ( _toMap.options & MAP_WAIT_NEUTRAL )
            && ! ( ( _toMap.options & MAP_PRESERVE_DIRS ) && ( mask & MASK_DIRS ) ) )
    {
        const uint8_t activeValue = _toMap.active.hats[hat];

        // Done mapping if the hat is centered again
        if ( value == 5 && activeValue != 5 )
        {
            doClearMapping ( _toMap.key );

            _joystickMappings.hats[hat][activeValue] = _toMap.key;

            // Set bit mask for neutral value
            _joystickMappings.hats[hat][5] = ( _joystickMappings.hats[hat][2] | _joystickMappings.hats[hat][4] |
                                               _joystickMappings.hats[hat][6] | _joystickMappings.hats[hat][8] );

            // Set bit masks for diagonal values
            _joystickMappings.hats[hat][1] = ( _joystickMappings.hats[hat][2] | _joystickMappings.hats[hat][4] );
            _joystickMappings.hats[hat][3] = ( _joystickMappings.hats[hat][2] | _joystickMappings.hats[hat][6] );
            _joystickMappings.hats[hat][7] = ( _joystickMappings.hats[hat][8] | _joystickMappings.hats[hat][4] );
            _joystickMappings.hats[hat][9] = ( _joystickMappings.hats[hat][8] | _joystickMappings.hats[hat][6] );

            _joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped hat%u value=%d to %08x", hat, activeValue, _toMap.key );

            Owner *owner = this->owner;
            const uint32_t key = _toMap.key;

            if ( _toMap.options & MAP_CONTINUOUSLY )
                _toMap.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue != 5 )
            return;

        _toMap.active.hats[hat] = value;
        return;
    }

    if ( ! mask )
        return;

    const uint32_t keyMapped = _joystickMappings.hats[hat][value];

    _state &= ~mask;

    if ( value != 5 )
        _state |= keyMapped;

    LOG_CONTROLLER ( this, "value=%u", value );
}

void Controller::joystickButtonEvent ( uint8_t button, uint8_t value )
{
    if ( _joystick.state.isNeutral() && ( _toMap.options & MAP_WAIT_NEUTRAL ) )
        _toMap.options &= ~MAP_WAIT_NEUTRAL;

    const uint32_t keyMapped = _joystickMappings.buttons[button];

    if ( _toMap.key != 0
            && ! ( _toMap.options & MAP_WAIT_NEUTRAL )
            && ! ( ( _toMap.options & MAP_PRESERVE_DIRS ) && ( keyMapped & MASK_DIRS ) ) )
    {
        const bool isActive = ( _toMap.active.buttons & ( 1u << button ) );

        // Done mapping if the button was released
        if ( !value && isActive )
        {
            doClearMapping ( _toMap.key );

            _joystickMappings.buttons[button] = _toMap.key;
            _joystickMappings.invalidate();

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", button, _toMap.key );

            Owner *owner = this->owner;
            const uint32_t key = _toMap.key;

            if ( _toMap.options & MAP_CONTINUOUSLY )
                _toMap.active.clear();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->controllerKeyMapped ( this, key );
        }

        // Otherwise ignore already active joystick buttons
        if ( isActive )
            return;

        _toMap.active.buttons |= ( 1u << button );
        return;
    }

    if ( keyMapped == 0 )
        return;

    if ( value )
        _state |= keyMapped;
    else
        _state &= ~keyMapped;

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

bool Controller::isUniqueName() const { return ( origNameCount[_origName] == 1 ); }

Controller::Controller ( KeyboardEnum ) : _origName ( "Keyboard" )
{
    _keyboardMappings.name = _origName;
    namesWithIndex.insert ( _keyboardMappings.name );
    origNameCount[_origName] = 1;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New keyboard" );
}

Controller::Controller ( const JoystickInfo& info ) : _origName ( info.name ), _joystick ( info )
{
    _joystickMappings.name = nextName ( _origName );
    namesWithIndex.insert ( _joystickMappings.name );

    const auto it = origNameCount.find ( _origName );
    if ( it == origNameCount.end() )
        origNameCount[_origName] = 1;
    else
        ++it->second;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New joystick: %s; %u axe(s); %u hat(s); %u button(s)",
                     _origName, _joystick.info.numAxes, _joystick.info.numHats, _joystick.info.numButtons );
}

Controller::~Controller()
{
    LOG_CONTROLLER ( this, "Deleting controller" );

    namesWithIndex.erase ( getName() );

    uint32_t& count = origNameCount[_origName];

    if ( count > 1 )
        --count;
    else
        origNameCount.erase ( _origName );

    KeyboardManager::get().unhook();
}

string Controller::getMapping ( uint32_t key, const string& placeholder ) const
{
    if ( key == _toMap.key && !placeholder.empty() )
        return placeholder;

    if ( isKeyboard() )
    {
        for ( uint8_t i = 0; i < 32; ++i )
            if ( ( key & ( 1u << i ) ) && _keyboardMappings.codes[i] )
                return _keyboardMappings.names[i];

        return "";
    }
    else if ( isJoystick() )
    {
        string mapping;

        for ( uint8_t axis = 0; axis < _joystick.info.numAxes; ++axis )
        {
            for ( uint8_t value = AXIS_POSITIVE; value <= AXIS_NEGATIVE; ++value )
            {
                if ( _joystickMappings.axes[axis][value] != key )
                    continue;

                const string str = format ( "%c %s", getAxisSign ( value ), _joystick.info.axisNames[axis] );

                if ( mapping.empty() )
                    mapping = str;
                else
                    mapping += ", " + str;
            }
        }

        for ( uint8_t hat = 0; hat < _joystick.info.numHats; ++hat )
        {
            for ( uint8_t value = 2; value <= 8; value += 2 )
            {
                if ( _joystickMappings.hats[hat][value] != key )
                    continue;

                string str = "DPad " + getHatString ( value );

                if ( hat > 0 )
                    str += format ( " (%u)", hat + 1 );

                if ( _origName.empty() )
                    mapping = str;
                else
                    mapping += ", " + str;
            }
        }

        for ( uint8_t button = 0; button < _joystick.info.numButtons; ++button )
        {
            if ( _joystickMappings.buttons[button] != key )
                continue;

            const string str = format ( "Button %u", button + 1 );

            if ( _origName.empty() )
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
                _keyboardMappings.codes[j] = vkCode;
                _keyboardMappings.names[j] = name;
            }
            else if ( _keyboardMappings.codes[j] == vkCode )
            {
                _keyboardMappings.codes[j] = 0;
                _keyboardMappings.names[j].clear();
            }
        }
    }

    _keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const KeyboardMappings& mappings )
{
    ASSERT ( isKeyboard() == true );

    LOG_CONTROLLER ( this, "KeyboardMappings" );

    _keyboardMappings = mappings;
    _keyboardMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const JoystickMappings& mappings )
{
    ASSERT ( isJoystick() == true );

    LOG_CONTROLLER ( this, "JoystickMappings" );

    _joystickMappings = mappings;
    _joystickMappings.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::startMapping ( Owner *owner, uint32_t key, uint8_t options )
{
    if ( _toMap.options & MAP_CONTINUOUSLY )
        _toMap.active.clear();
    else
        cancelMapping();

    LOG_CONTROLLER ( this, "Starting mapping %08x", key );

    if ( ! _joystick.state.isNeutral() )
        options |= MAP_WAIT_NEUTRAL;

    this->owner = owner;
    _toMap.key = key;
    _toMap.options = options;
}

void Controller::cancelMapping()
{
    LOG_CONTROLLER ( this, "Cancel mapping %08x", _toMap.key );

    owner = 0;
    _toMap.key = 0;
    _toMap.options = 0;
    _toMap.active.clear();
}

void Controller::doClearMapping ( uint32_t keys )
{
    for ( uint8_t i = 0; i < 32; ++i )
    {
        if ( keys & ( 1u << i ) )
        {
            _keyboardMappings.codes[i] = 0;
            _keyboardMappings.names[i].clear();
            _keyboardMappings.invalidate();
        }
    }

    for ( auto& a : _joystickMappings.axes )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                _joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : _joystickMappings.hats )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                _joystickMappings.invalidate();
            }
        }
    }

    for ( auto& a : _joystickMappings.buttons )
    {
        if ( a & keys )
        {
            a = 0;
            _joystickMappings.invalidate();
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
    for ( uint8_t axis = 0; axis < _joystick.info.numAxes; ++axis )
    {
        if ( _joystick.info.axisNames[axis].empty() )
            continue;

        if ( _joystick.info.axisNames[axis][0] == 'X' )
        {
            _joystickMappings.axes[axis][AXIS_CENTERED] = MASK_X_AXIS;
            _joystickMappings.axes[axis][AXIS_POSITIVE] = BIT_RIGHT;
            _joystickMappings.axes[axis][AXIS_NEGATIVE] = BIT_LEFT;
        }
        else if ( _joystick.info.axisNames[axis][0] == 'Y' )
        {
            _joystickMappings.axes[axis][AXIS_CENTERED] = MASK_Y_AXIS;
            _joystickMappings.axes[axis][AXIS_POSITIVE] = BIT_UP;
            _joystickMappings.axes[axis][AXIS_NEGATIVE] = BIT_DOWN;
        }
    }

    // Default hat mappings
    for ( uint8_t hat = 0; hat < _joystick.info.numHats; ++hat )
    {
        _joystickMappings.hats[hat][5] = MASK_DIRS;
        _joystickMappings.hats[hat][8] = BIT_UP;
        _joystickMappings.hats[hat][9] = BIT_UP | BIT_RIGHT;
        _joystickMappings.hats[hat][6] = BIT_RIGHT;
        _joystickMappings.hats[hat][3] = BIT_DOWN | BIT_RIGHT;
        _joystickMappings.hats[hat][2] = BIT_DOWN;
        _joystickMappings.hats[hat][1] = BIT_DOWN | BIT_LEFT;
        _joystickMappings.hats[hat][4] = BIT_LEFT;
        _joystickMappings.hats[hat][7] = BIT_UP | BIT_LEFT;
    }

    // Clear all buttons
    doClearMapping ( MASK_BUTTONS );

    // Default deadzone
    _joystickMappings.deadzone = DEFAULT_DEADZONE;

    _joystickMappings.invalidate();
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
        return ControllerManager::saveMappings ( file, _keyboardMappings );
    else
        return ControllerManager::saveMappings ( file, _joystickMappings );
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

        if ( msg->getAs<KeyboardMappings>().name != _keyboardMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != keyboard '%s'",
                  msg->getAs<KeyboardMappings>().name, _keyboardMappings.name );
        }

        _keyboardMappings = msg->getAs<KeyboardMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }
    else // if ( isJoystick() )
    {
        if ( msg->getMsgType() != MsgType::JoystickMappings )
        {
            LOG ( "Invalid joystick mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<JoystickMappings>().name != _joystickMappings.name )
        {
            LOG ( "Name mismatch: decoded '%s' != joystick '%s'",
                  msg->getAs<JoystickMappings>().name, _joystickMappings.name );
        }

        _joystickMappings = msg->getAs<JoystickMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }

    return true;
}
