#include "Controller.h"
#include "ControllerManager.h"
#include "Constants.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#include <SDL.h>
#include <windows.h>

#include <cstdlib>
#include <limits>

using namespace std;


#define EVENT_JOY_AXIS      ( 0 )
#define EVENT_JOY_HAT       ( 1 )
#define EVENT_JOY_BUTTON    ( 2 )

#define AXIS_CENTERED       ( 0 )
#define AXIS_POSITIVE       ( 1 )
#define AXIS_NEGATIVE       ( 2 )


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

static char getAxisSign ( int index, int value )
{
    if ( value == 0 )
        return '0';
    else if ( index % 2 == 0 || index > 2 )
        return ( value == AXIS_POSITIVE ? '+' : '-' );
    else
        return ( value == AXIS_POSITIVE ? '-' : '+' ); // SDL joystick Y-axis is inverted
}

void Controller::joystickEvent ( const SDL_JoyAxisEvent& event )
{
    uint32_t *values = stick.mappings[EVENT_JOY_AXIS][event.axis];

    uint8_t value = 0;
    if ( abs ( event.value ) > stick.deadzone )
        value = ( event.value > 0 ? AXIS_POSITIVE : AXIS_NEGATIVE );

    if ( keyToMap != 0 && !waitForNeutral
            && ( ! ( options & MAP_PRESERVE_DIRS ) || ! ( values[AXIS_CENTERED] & MASK_DIRS ) ) )
    {
        uint32_t *activeValues = active.mappings[EVENT_JOY_AXIS][event.axis];

        uint8_t activeValue = 0;

        if ( activeValues[AXIS_POSITIVE] )
            activeValue = AXIS_POSITIVE;
        else if ( activeValues[AXIS_NEGATIVE] )
            activeValue = AXIS_NEGATIVE;

        // Done mapping if the axis returned to 0
        if ( value == 0 && activeValue )
        {
            doClearMapping ( keyToMap );

            values[activeValue] = keyToMap;

            // Set bit mask for neutral value
            values[AXIS_CENTERED] = ( values[AXIS_POSITIVE] | values[AXIS_NEGATIVE] );

            stick.invalidate();

            LOG_CONTROLLER ( this, "Mapped axis%d %c to %08x", event.axis, getAxisSign ( 0, value ), keyToMap );

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

        activeValues[value] = keyToMap;
        return;
    }

    const uint32_t mask = values[AXIS_CENTERED];

    if ( !mask )
        return;

    state &= ~mask;

    if ( value != AXIS_CENTERED )
        state |= values[value];

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    // LOG_CONTROLLER ( this, "axis=%d; value=%c; EVENT_JOY_AXIS", event.axis, getAxisSign ( 0, value ) );
}

static int getHatNumPadDir ( int hat )
{
    int dir = 5;

    if ( hat != SDL_HAT_CENTERED )
    {
        if ( hat & SDL_HAT_UP )
            dir = 8;
        else if ( hat & SDL_HAT_DOWN )
            dir = 2;

        if ( hat & SDL_HAT_LEFT )
            --dir;
        else if ( hat & SDL_HAT_RIGHT )
            ++dir;
    }

    return dir;
}

void Controller::joystickEvent ( const SDL_JoyHatEvent& event )
{
    uint32_t *values = stick.mappings[EVENT_JOY_HAT][event.hat];

    if ( keyToMap != 0 && !waitForNeutral
            && ( ! ( options & MAP_PRESERVE_DIRS ) || ! ( values[SDL_HAT_CENTERED] & MASK_DIRS ) ) )
    {
        uint32_t *activeValues = active.mappings[EVENT_JOY_HAT][event.hat];

        uint8_t activeValue = 0;

        if ( activeValues[SDL_HAT_UP] )
            activeValue = SDL_HAT_UP;
        else if ( activeValues[SDL_HAT_RIGHT] )
            activeValue = SDL_HAT_RIGHT;
        else if ( activeValues[SDL_HAT_DOWN] )
            activeValue = SDL_HAT_DOWN;
        else if ( activeValues[SDL_HAT_LEFT] )
            activeValue = SDL_HAT_LEFT;

        // Done mapping if the hat is centered
        if ( event.value == SDL_HAT_CENTERED && activeValue )
        {
            doClearMapping ( keyToMap );

            values[activeValue] = activeValues[activeValue];

            // Set bit mask for centered value
            values[SDL_HAT_CENTERED] = 0;
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_UP];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_RIGHT];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_DOWN];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_LEFT];

            stick.invalidate();

            LOG_CONTROLLER ( this, "Mapped hat%d %d to %08x", event.hat, getHatNumPadDir ( activeValue ), keyToMap );

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

        activeValues[event.value] = keyToMap;
        return;
    }

    const uint32_t mask = values[SDL_HAT_CENTERED];

    if ( !mask )
        return;

    state &= ~mask;

    if ( event.value & SDL_HAT_UP )
        state |= values[SDL_HAT_UP];
    else if ( event.value & SDL_HAT_DOWN )
        state |= values[SDL_HAT_DOWN];

    if ( event.value & SDL_HAT_LEFT )
        state |= values[SDL_HAT_LEFT];
    else if ( event.value & SDL_HAT_RIGHT )
        state |= values[SDL_HAT_RIGHT];

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    // LOG_CONTROLLER ( this, "hat=%d; value=%d; EVENT_JOY_HAT", event.hat, getHatNumPadDir ( event.value ) );
}

void Controller::joystickEvent ( const SDL_JoyButtonEvent& event )
{
    uint32_t *values = stick.mappings[EVENT_JOY_BUTTON][event.button];

    if ( keyToMap != 0 && !waitForNeutral
            && ( ! ( options & MAP_PRESERVE_DIRS ) || ! ( values[SDL_PRESSED] & MASK_DIRS ) ) )
    {
        uint32_t *activeStates = active.mappings[EVENT_JOY_BUTTON][event.button];
        const bool isActive = ( activeStates[SDL_PRESSED] );

        // Done mapping if the button was released
        if ( event.state == SDL_RELEASED && isActive )
        {
            doClearMapping ( keyToMap );

            values[SDL_PRESSED] = values[SDL_RELEASED] = keyToMap;

            stick.invalidate();

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", event.button, keyToMap );

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
        if ( isActive )
            return;

        activeStates[event.state] = keyToMap;
        return;
    }

    if ( event.state == SDL_RELEASED )
        anyButton &= ~ ( 1u << event.button );
    else if ( event.state == SDL_PRESSED )
        anyButton |= ( 1u << event.button );

    const uint32_t key = values[event.state];

    if ( key == 0 )
        return;

    if ( event.state == SDL_RELEASED )
        state &= ~key;
    else if ( event.state == SDL_PRESSED )
        state |= key;

    if ( !state && waitForNeutral )
        waitForNeutral = false;

    // LOG_CONTROLLER ( this, "button=%d; value=%d; EVENT_JOY_BUTTON", event.button, ( event.state == SDL_PRESSED ) );
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

Controller::Controller ( KeyboardEnum ) : name ( "Keyboard" )
{
    keybd.name = name;
    namesWithIndex.insert ( keybd.name );
    origNameCount[name] = 1;

    doClearMapping();
    doResetToDefaults();

    LOG_CONTROLLER ( this, "New keyboard" );
}

Controller::Controller ( SDL_Joystick *joystick ) : name ( SDL_JoystickName ( joystick ) ), joystick ( joystick )
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

    if ( joystick )
        SDL_JoystickClose ( joystick );

    namesWithIndex.erase ( getName() );

    uint32_t& count = origNameCount[name];

    if ( count > 1 )
        --count;
    else
        origNameCount.erase ( name );
}

bool Controller::isUniqueName() const { return ( origNameCount[name] == 1 ); }

static string getHatString ( int hat )
{
    if ( hat == SDL_HAT_CENTERED )
        return "Centered";

    string dir;

    if ( hat & SDL_HAT_UP )
        dir = "Up";
    else if ( hat & SDL_HAT_DOWN )
        dir = "Down";

    if ( hat & SDL_HAT_LEFT )
        dir += "Left";
    else if ( hat & SDL_HAT_RIGHT )
        dir += "Right";

    return dir;
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
        string ret;

        for ( uint8_t type = 0; type < 3; ++type )
        {
            for ( uint16_t index = 0; index < 256; ++index )
            {
                for ( uint8_t value = 1; value < 16; ++value ) // We skip value=0 since it is the neutral value
                {
                    if ( stick.mappings[type][index][value] == key )
                    {
                        string mapping;

                        switch ( type )
                        {
                            case EVENT_JOY_AXIS:
                                if ( index <= 2 )
                                    mapping = format ( "%c %c-Axis", getAxisSign ( index, value ), 'X' + index );
                                else
                                    mapping = format ( "%c Axis (%u)", getAxisSign ( index, value ), index + 1 );
                                break;

                            case EVENT_JOY_HAT:
                                if ( index == 0 )
                                    mapping = format ( "DPad %s", getHatString ( value ) );
                                else
                                    mapping = format ( "DPad (%u) %s", index + 1, getHatString ( value ) );
                                break;

                            case EVENT_JOY_BUTTON:
                                mapping = format ( "Button %u", index + 1 );
                                break;

                            default:
                                ASSERT_IMPOSSIBLE;
                        }

                        if ( ret.empty() )
                            ret = mapping;
                        else
                            ret += ", " + mapping;
                    }
                }
            }
        }

        return ret;
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

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const KeyboardMappings& mappings )
{
    LOG_CONTROLLER ( this, "KeyboardMappings" );

    keybd = mappings;

    ControllerManager::get().mappingsChanged ( this );
}

void Controller::setMappings ( const JoystickMappings& mappings )
{
    LOG_CONTROLLER ( this, "JoystickMappings" );

    stick = mappings;

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
        KeyboardManager::get().hook ( this, window, {}, ignore ); // Check all except ignored keys
    else
        KeyboardManager::get().hook ( this, window, { VK_ESCAPE }, ignore ); // Only check ESC key
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
    for ( auto& a : active.mappings )
    {
        for ( auto& b : a )
        {
            for ( auto& c : b )
                c = 0;
        }
    }
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

    for ( auto& a : stick.mappings )
    {
        for ( auto& b : a )
        {
            for ( auto& c : b )
            {
                if ( c & keys )
                {
                    c = 0;
                    stick.invalidate();
                }
            }
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
    stick.mappings[EVENT_JOY_AXIS][0][0] = MASK_X_AXIS;
    stick.mappings[EVENT_JOY_AXIS][0][AXIS_POSITIVE] = BIT_RIGHT;
    stick.mappings[EVENT_JOY_AXIS][0][AXIS_NEGATIVE] = BIT_LEFT;
    stick.mappings[EVENT_JOY_AXIS][1][0] = MASK_Y_AXIS;
    stick.mappings[EVENT_JOY_AXIS][1][AXIS_POSITIVE] = BIT_DOWN; // SDL joystick Y-axis is inverted
    stick.mappings[EVENT_JOY_AXIS][1][AXIS_NEGATIVE] = BIT_UP;

    // Default hat mappings
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_CENTERED] = ( MASK_X_AXIS | MASK_Y_AXIS );
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_UP]       = BIT_UP;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_RIGHT]    = BIT_RIGHT;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_DOWN]     = BIT_DOWN;
    stick.mappings[EVENT_JOY_HAT][0][SDL_HAT_LEFT]     = BIT_LEFT;

    // Default deadzone
    stick.deadzone = DEFAULT_DEADZONE;
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
