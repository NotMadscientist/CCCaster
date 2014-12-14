#include "Controller.h"
#include "ControllerManager.h"
#include "Constants.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#include <allegro5/allegro.h>
#include <windows.h>

#include <cstdlib>
#include <limits>

using namespace std;


static string getVKeyName ( uint32_t vkCode, uint32_t scanCode, bool isExtended )
{
    switch ( vkCode )
    {
#include "KeyboardMappings.h"

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

void Controller::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
    // Ignore keyboard events for joystick (except ESC)
    if ( isJoystick() && vkCode != VK_ESCAPE )
        return;

    // Only use keyboard down events for mapping
    if ( !isDown && keyToMap != 0 )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        doClearMapping ( keyToMap );

        const string name = getVKeyName ( vkCode, scanCode, isExtended );

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( keyToMap & ( 1u << i ) )
            {
                keybd.codes[i] = vkCode;
                keybd.names[i] = name;
            }
            else if ( keybd.codes[i] == vkCode )
            {
                keybd.codes[i] = 0;
                keybd.names[i].clear();
            }
        }

        keybd.invalidate();
        key = keyToMap;

        LOG_CONTROLLER ( this, "Mapped key [0x%02X] %s to %08x ", vkCode, name, keyToMap );
    }

    if ( options & MAP_CONTINUOUSLY )
        clearActive();
    else
        cancelMapping();

    ControllerManager::get().mappingsChanged ( this );

    if ( owner )
        owner->doneMapping ( this, key );
}

uint8_t combineAxis ( char type, uint8_t index )
{
    ASSERT ( type >= 'X' && type <= 'Z' );

    ASSERT ( index < 64 );

    return ( ( type - 'X' ) & 3 ) | ( index << 2 );
}

static void splitAxis ( uint8_t axis, char& type, uint8_t& index )
{
    type = 'X' + ( axis & 3 );

    index = ( axis >> 2 );
}

static char getAxisSign ( uint8_t axis, uint8_t value )
{
    if ( value == AXIS_CENTERED )
        return '0';

    char type;
    uint8_t index;
    splitAxis ( axis, type, index );

    if ( type == 'Y' )
        return ( value == AXIS_POSITIVE ? '-' : '+' );  // Allegro joystick Y-axis is inverted
    else
        return ( value == AXIS_POSITIVE ? '+' : '-' );
}

void Controller::joystickAxisEvent ( uint8_t axis, uint8_t value )
{
    const uint32_t keyMapped = stick.axes[axis][value];

    if ( keyToMap != 0 && !waitForNeutral
            && ! ( ( options & MAP_PRESERVE_DIRS ) && ( stick.axes[axis][AXIS_CENTERED] & MASK_DIRS ) ) )
    {
        uint8_t activeValue = 0;
        if ( active.axes[axis][AXIS_POSITIVE] )
            activeValue = AXIS_POSITIVE;
        else if ( active.axes[axis][AXIS_NEGATIVE] )
            activeValue = AXIS_NEGATIVE;

        // Done mapping if the axis returned to 0
        if ( value == AXIS_CENTERED && activeValue )
        {
            doClearMapping ( keyToMap );

            stick.axes[axis][activeValue] = keyToMap;

            // Set bit mask for neutral value
            stick.axes[axis][AXIS_CENTERED] = ( stick.axes[axis][AXIS_POSITIVE] | stick.axes[axis][AXIS_NEGATIVE] );

            stick.invalidate();

            LOG_CONTROLLER ( this, "Mapped value=%c to %08x", getAxisSign ( axis, activeValue ), keyToMap );

            Owner *owner = this->owner;
            const uint32_t key = keyToMap;

            if ( options & MAP_CONTINUOUSLY )
                clearActive();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick mappings
        if ( activeValue )
            return;

        active.axes[axis][value] = keyToMap;
        return;
    }

    const uint32_t mask = stick.axes[axis][AXIS_CENTERED];

    if ( !mask )
        return;

    state &= ~mask;

    if ( value != AXIS_CENTERED )
        state |= keyMapped;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    LOG_CONTROLLER ( this, "value=%c", getAxisSign ( axis, value ) );
}

void Controller::joystickButtonEvent ( uint8_t button, bool isDown )
{
    const uint32_t keyMapped = stick.buttons[button];

    if ( keyToMap != 0 && !waitForNeutral
            && ! ( ( options & MAP_PRESERVE_DIRS ) && ( keyMapped & MASK_DIRS ) ) )
    {
        const bool isActive = ( active.buttons[button] != 0 );

        // Done mapping if the button was released
        if ( !isDown && isActive )
        {
            doClearMapping ( keyToMap );

            stick.buttons[button] = keyToMap;

            stick.invalidate();

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", button, keyToMap );

            Owner *owner = this->owner;
            const uint32_t key = keyToMap;

            if ( options & MAP_CONTINUOUSLY )
                clearActive();
            else
                cancelMapping();

            ControllerManager::get().mappingsChanged ( this );

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active joystick buttons
        if ( isActive )
            return;

        active.buttons[button] = keyToMap;
        return;
    }

    if ( isDown )
        anyButton |= ( 1u << button );
    else
        anyButton &= ~ ( 1u << button );

    if ( keyMapped == 0 )
        return;

    if ( isDown )
        state |= keyMapped;
    else
        state &= ~keyMapped;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    LOG_CONTROLLER ( this, "button=%d; isDown=%d", button, isDown );
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
    keybd.name = name;
    namesWithIndex.insert ( keybd.name );
    origNameCount[name] = 1;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New keyboard" );
}

Controller::Controller ( void *joystick )
    : name ( al_get_joystick_name ( ( ALLEGRO_JOYSTICK * ) joystick ) )
    , joystick ( joystick )
{
    stick.name = nextName ( name );
    namesWithIndex.insert ( stick.name );

    auto it = origNameCount.find ( name );
    if ( it == origNameCount.end() )
        origNameCount[name] = 1;
    else
        ++it->second;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New joystick: origName=%s", name );
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
}

string Controller::getMapping ( uint32_t key, const string& placeholder ) const
{
    if ( key == keyToMap && !placeholder.empty() )
        return placeholder;

    if ( isKeyboard() )
    {
        for ( uint8_t i = 0; i < 32; ++i )
            if ( key & ( 1u << i ) && keybd.codes[i] )
                return keybd.names[i];

        return "";
    }
    else if ( isJoystick() )
    {
        string mapping;

        for ( uint16_t axis = 0; axis < 256; ++axis )
        {
            for ( uint8_t value = AXIS_POSITIVE; value <= AXIS_NEGATIVE; ++value )
            {
                if ( stick.axes[axis][value] != key )
                    continue;

                char type;
                uint8_t index;
                splitAxis ( axis, type, index );

                string str;
                if ( index == 0 )
                    str = format ( "%c %c-Axis", getAxisSign ( axis, value ), type );
                else
                    str = format ( "%c %c-Axis (%u)", getAxisSign ( axis, value ), type, index + 1 );

                if ( mapping.empty() )
                    mapping = str;
                else
                    mapping += ", " + str;
            }
        }

        for ( uint16_t button = 0; button < 256; ++button )
        {
            if ( stick.buttons[button] != key )
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

    for ( uint8_t i = 0; i < 10; ++i )
    {
        const uint32_t vkCode = MapVirtualKey ( config[i], MAPVK_VSC_TO_VK_EX );
        const string name = getVKeyName ( vkCode, config[i], false );

        for ( uint8_t j = 0; j < 32; ++j )
        {
            if ( bits[i] & ( 1u << j ) )
            {
                keybd.codes[j] = vkCode;
                keybd.names[j] = name;
            }
            else if ( keybd.codes[j] == vkCode )
            {
                keybd.codes[j] = 0;
                keybd.names[j].clear();
            }
        }
    }

    keybd.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const KeyboardMappings& mappings )
{
    LOG_CONTROLLER ( this, "KeyboardMappings" );

    keybd = mappings;
    keybd.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const JoystickMappings& mappings )
{
    LOG_CONTROLLER ( this, "JoystickMappings" );

    stick = mappings;
    stick.invalidate();

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::startMapping ( Owner *owner, uint32_t key, const void *window,
                                const unordered_set<uint32_t>& ignore, uint8_t options )
{
    if ( this->options & MAP_CONTINUOUSLY )
        clearActive();
    else
        cancelMapping();

    LOG ( "Starting mapping %08x", key );

    if ( state )
        waitForNeutral = true;

    this->owner = owner;
    this->keyToMap = key;
    this->options = options;

    if ( isKeyboard() )
        KeyboardManager::get().matchedKeys.clear(); // Check all except ignored keys
    else
        KeyboardManager::get().matchedKeys = { VK_ESCAPE }; // Only check ESC key

    KeyboardManager::get().ignoredKeys = ignore;
    KeyboardManager::get().hook ( this );
}

void Controller::cancelMapping()
{
    LOG ( "Cancel mapping %08x", keyToMap );

    KeyboardManager::get().unhook();

    owner = 0;
    keyToMap = 0;
    waitForNeutral = false;

    clearActive();
}

void Controller::clearActive()
{
    for ( auto& a : active.axes )
        for ( auto& b : a )
            b = 0;

    for ( auto& a : active.buttons )
        a = 0;
}

void Controller::doClearMapping ( uint32_t keys )
{
    for ( uint8_t i = 0; i < 32; ++i )
    {
        if ( keys & ( 1u << i ) )
        {
            keybd.codes[i] = 0;
            keybd.names[i].clear();
            keybd.invalidate();
        }
    }

    for ( auto& a : stick.axes )
    {
        for ( auto& b : a )
        {
            if ( b & keys )
            {
                b = 0;
                stick.invalidate();
            }
        }
    }

    for ( auto& a : stick.buttons )
    {
        if ( a & keys )
        {
            a = 0;
            stick.invalidate();
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
    if ( !isJoystick() )
        return;

    // Clear all buttons
    doClearMapping ( MASK_BUTTONS );

    // Default axis mappings
    for ( uint8_t i = 0; i < 3; ++i )
    {
        stick.axes[ combineAxis ( 'X', i ) ][AXIS_CENTERED] = MASK_X_AXIS;
        stick.axes[ combineAxis ( 'X', i ) ][AXIS_POSITIVE] = BIT_RIGHT;
        stick.axes[ combineAxis ( 'X', i ) ][AXIS_NEGATIVE] = BIT_LEFT;
        stick.axes[ combineAxis ( 'Y', i ) ][AXIS_CENTERED] = MASK_Y_AXIS;
        stick.axes[ combineAxis ( 'Y', i ) ][AXIS_POSITIVE] = BIT_DOWN; // Allegro joystick Y-axis is inverted
        stick.axes[ combineAxis ( 'Y', i ) ][AXIS_NEGATIVE] = BIT_UP;
    }

    // Default deadzone
    stick.deadzone = DEFAULT_DEADZONE;

    stick.invalidate();
}

void Controller::resetToDefaults()
{
    if ( !isJoystick() )
        return;

    doResetToDefaults();

    ControllerManager::get().mappingsChanged ( this );
}

bool Controller::saveMappings ( const string& file ) const
{
    if ( isKeyboard() )
        return ControllerManager::saveMappings ( file, keybd );
    else
        return ControllerManager::saveMappings ( file, stick );
}

bool Controller::loadMappings ( const string& file )
{
    MsgPtr msg = ControllerManager::loadMappings ( file );

    if ( !msg )
        return false;

    if ( isKeyboard() )
    {
        if ( msg->getMsgType() != MsgType::KeyboardMappings )
        {
            LOG ( "Invalid keyboard mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<KeyboardMappings>().name != keybd.name )
        {
            LOG ( "Name mismatch: decoded '%s' != keyboard '%s'",
                  msg->getAs<KeyboardMappings>().name, keybd.name );
        }

        keybd = msg->getAs<KeyboardMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }
    else // if ( isJoystick() )
    {
        if ( msg->getMsgType() != MsgType::JoystickMappings )
        {
            LOG ( "Invalid joystick mapping type: %s", msg->getMsgType() );
            return false;
        }

        if ( msg->getAs<JoystickMappings>().name != stick.name )
        {
            LOG ( "Name mismatch: decoded '%s' != joystick '%s'",
                  msg->getAs<JoystickMappings>().name, stick.name );
        }

        stick = msg->getAs<JoystickMappings>();
        ControllerManager::get().mappingsChanged ( this );
    }

    return true;
}
