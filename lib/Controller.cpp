#include "Controller.h"
#include "Logger.h"

#include <SDL.h>
#include <windows.h>

#include <cstdlib>

using namespace std;


#define DEFAULT_DEADZONE    25000

#define EVENT_JOY_AXIS      0
#define EVENT_JOY_HAT       1
#define EVENT_JOY_BUTTON    2

#define AXIS_CENTERED       0
#define AXIS_POSITIVE       1
#define AXIS_NEGATIVE       2


static string getVKeyName ( int vkCode )
{
    uint32_t scanCode = MapVirtualKey ( vkCode, MAPVK_VK_TO_VSC );

    // MapVirtualKey strips the extended bit for some keys
    switch ( vkCode )
    {
#include "KeyboardMappings.h"

        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
            scanCode |= 0x100; // Set extended bit
            break;

        default:
            break;
    }

    char name[4096];

    if ( GetKeyNameText ( scanCode << 16, name, sizeof ( name ) ) > 0 )
        return name;
    else
        return toString ( "Key Code %d", vkCode );
}

void Controller::keyboardEvent ( int vkCode, bool isDown )
{
    if ( !isDown )
        return;

    Owner *owner = this->owner;
    uint32_t key = 0;

    if ( vkCode != VK_ESCAPE )
    {
        size_t bit = 0xFFFFFFFF;

        for ( size_t i = 0; i < 32; ++i )
        {
            if ( bit > 32 && keyToMap & ( 1u << i ) )
                bit = i;

            if ( keyboard[i] == vkCode )
                keyboard[i] = 0;
        }

        ASSERT ( bit < 32 );

        keyboard[bit] = vkCode;
        key = keyToMap;
    }

    cancelMapping();

    if ( owner )
        owner->doneMapping ( this, key );
}

void Controller::joystickEvent ( const SDL_JoyAxisEvent& event )
{
    uint32_t *values = mappings[EVENT_JOY_AXIS][event.axis];

    uint8_t value = 0;
    if ( abs ( event.value ) > deadzones[event.axis] )
        value = ( event.value > 0 ? AXIS_POSITIVE : AXIS_NEGATIVE );

    if ( keyToMap != 0 )
    {
        uint32_t *activeValues = activeMappings[EVENT_JOY_AXIS][event.axis];

        uint8_t activeValue = 0;

        if ( activeValues[AXIS_POSITIVE] )
            activeValue = AXIS_POSITIVE;
        else if ( activeValues[AXIS_NEGATIVE] )
            activeValue = AXIS_NEGATIVE;

        if ( value == 0 && activeValue )
        {
            // Done mapping if the axis returned to 0
            values[activeValue] = keyToMap;

            values[AXIS_CENTERED] = ( values[AXIS_POSITIVE] | values[AXIS_NEGATIVE] );

            LOG_CONTROLLER ( this, "Mapped axis%d %s to %08x",
                             event.axis, ( activeValue == AXIS_POSITIVE ? "+" : "-" ), keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active mappings
        if ( activeValue )
            return;

        activeValues[value] = keyToMap;
        return;
    }

    state &= ~values[AXIS_CENTERED];

    if ( value != AXIS_CENTERED )
        state |= values[value];

    LOG_CONTROLLER ( this, "axis=%d; value=%s; EVENT_JOY_AXIS",
                     event.axis, ( value == 0 ? "0" : ( value == AXIS_POSITIVE ? "+" : "-" ) ) );
}

static int ConvertHatToNumPad ( int hat )
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
    uint32_t *values = mappings[EVENT_JOY_HAT][event.hat];

    if ( keyToMap != 0 )
    {
        uint32_t *activeValues = activeMappings[EVENT_JOY_HAT][event.hat];

        uint8_t activeValue = 0;

        if ( activeValues[SDL_HAT_UP] )
            activeValue = SDL_HAT_UP;
        else if ( activeValues[SDL_HAT_RIGHT] )
            activeValue = SDL_HAT_RIGHT;
        else if ( activeValues[SDL_HAT_DOWN] )
            activeValue = SDL_HAT_DOWN;
        else if ( activeValues[SDL_HAT_LEFT] )
            activeValue = SDL_HAT_LEFT;

        if ( event.value == SDL_HAT_CENTERED && activeValue )
        {
            // Done mapping if the hat is centered
            values[activeValue] = activeValues[activeValue];

            values[SDL_HAT_CENTERED] = 0;
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_UP];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_RIGHT];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_DOWN];
            values[SDL_HAT_CENTERED] |= values[SDL_HAT_LEFT];

            LOG_CONTROLLER ( this, "Mapped hat%d %d to %08x",
                             event.hat, ConvertHatToNumPad ( activeValue ), keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active mappings
        if ( activeValue )
            return;

        activeValues[event.value] = keyToMap;
        return;
    }

    state &= ~values[SDL_HAT_CENTERED];

    if ( event.value & SDL_HAT_UP )
        state |= values[SDL_HAT_UP];
    else if ( event.value & SDL_HAT_DOWN )
        state |= values[SDL_HAT_DOWN];

    if ( event.value & SDL_HAT_LEFT )
        state |= values[SDL_HAT_LEFT];
    else if ( event.value & SDL_HAT_RIGHT )
        state |= values[SDL_HAT_RIGHT];

    LOG_CONTROLLER ( this, "hat=%d; value=%d; EVENT_JOY_HAT", event.hat, ConvertHatToNumPad ( event.value ) );
}

void Controller::joystickEvent ( const SDL_JoyButtonEvent& event )
{
    if ( keyToMap != 0 )
    {
        uint32_t *activeStates = activeMappings[EVENT_JOY_BUTTON][event.button];
        bool isActive = ( activeStates[SDL_PRESSED] );

        if ( event.state == SDL_RELEASED && isActive )
        {
            // Done mapping if the button was tapped
            mappings[EVENT_JOY_BUTTON][event.button][SDL_PRESSED]
                = mappings[EVENT_JOY_BUTTON][event.button][SDL_RELEASED] = keyToMap;

            LOG_CONTROLLER ( this, "Mapped button%d to %08x", event.button, keyToMap );

            Owner *owner = this->owner;
            uint32_t key = keyToMap;

            cancelMapping();

            if ( owner )
                owner->doneMapping ( this, key );
        }

        // Otherwise ignore already active mappings
        if ( isActive )
            return;

        activeStates[event.state] = keyToMap;
        return;
    }

    uint32_t key = mappings[EVENT_JOY_BUTTON][event.button][event.state];

    if ( key == 0 )
        return;

    if ( event.state == SDL_RELEASED )
        state &= ~key;
    else if ( event.state == SDL_PRESSED )
        state |= key;

    LOG_CONTROLLER ( this, "button=%d; value=%d; EVENT_JOY_BUTTON", event.button, ( event.state == SDL_PRESSED ) );
}

Controller::Controller ( KeyboardEnum ) : name ( "Keyboard" )
{
    memset ( &guid, 0, sizeof ( guid ) );

    clearMapping();

    // TODO get default keyboard mappings from game
}

Controller::Controller ( SDL_Joystick *joystick ) : joystick ( joystick ), name ( SDL_JoystickName ( joystick ) )
{
    SDL_JoystickGUID guid = SDL_JoystickGetGUID ( joystick );
    memcpy ( &this->guid.guid, guid.data, sizeof ( guid.data ) );

    auto it = guidBitset.find ( this->guid.guid );

    if ( it == guidBitset.end() )
    {
        this->guid.index = 0;
        guidBitset[this->guid.guid] = 1u;
    }
    else
    {
        for ( uint32_t i = 0; i < 32; ++i )
        {
            if ( ( it->second & ( 1u << i ) ) == 0u )
            {
                guidBitset[this->guid.guid] |= ( 1u << i );
                this->guid.index = i;
                return;
            }
        }

        LOG_AND_THROW_STRING ( "Too many duplicate guids for: '%s'", this->guid.guid );
    }

    clearMapping();

    for ( auto& v : deadzones )
        v = DEFAULT_DEADZONE;

    // Default axis mappings
    mappings[EVENT_JOY_AXIS][0][0] = MASK_X_AXIS;
    mappings[EVENT_JOY_AXIS][0][AXIS_POSITIVE] = BIT_RIGHT;
    mappings[EVENT_JOY_AXIS][0][AXIS_NEGATIVE] = BIT_LEFT;
    mappings[EVENT_JOY_AXIS][1][0] = MASK_Y_AXIS;
    mappings[EVENT_JOY_AXIS][1][AXIS_POSITIVE] = BIT_DOWN; // SDL joystick Y-axis is inverted
    mappings[EVENT_JOY_AXIS][1][AXIS_NEGATIVE] = BIT_UP;
    mappings[EVENT_JOY_AXIS][2][0] = MASK_X_AXIS;
    mappings[EVENT_JOY_AXIS][2][AXIS_POSITIVE] = BIT_RIGHT;
    mappings[EVENT_JOY_AXIS][2][AXIS_NEGATIVE] = BIT_LEFT;
    mappings[EVENT_JOY_AXIS][3][0] = MASK_Y_AXIS;
    mappings[EVENT_JOY_AXIS][3][AXIS_POSITIVE] = BIT_DOWN; // SDL joystick Y-axis is inverted
    mappings[EVENT_JOY_AXIS][3][AXIS_NEGATIVE] = BIT_UP;

    // Default hat mappings
    mappings[EVENT_JOY_HAT][0][SDL_HAT_CENTERED] = ( MASK_X_AXIS | MASK_Y_AXIS );
    mappings[EVENT_JOY_HAT][0][SDL_HAT_UP]       = BIT_UP;
    mappings[EVENT_JOY_HAT][0][SDL_HAT_RIGHT]    = BIT_RIGHT;
    mappings[EVENT_JOY_HAT][0][SDL_HAT_DOWN]     = BIT_DOWN;
    mappings[EVENT_JOY_HAT][0][SDL_HAT_LEFT]     = BIT_LEFT;
}

Controller::~Controller()
{
    auto it = guidBitset.find ( guid.guid );

    if ( it == guidBitset.end() )
        return;

    if ( guid.index >= 32 )
        return;

    guidBitset[guid.guid] &= ~ ( 1u << guid.index );
}

string Controller::getMapping ( uint32_t key ) const
{
    if ( isKeyboard() )
    {
        size_t i;

        for ( i = 0; i < 32; ++i )
            if ( key & ( 1u << i ) )
                break;

        ASSERT ( i < 32 );

        if ( keyboard[i] )
            return getVKeyName ( keyboard[i] );
        else
            return "";
    }

    // TODO joystick
    return "";
}

void Controller::startMapping ( Owner *owner, uint32_t key, const void *window )
{
    cancelMapping();

    LOG ( "Starting mapping %08x", key );

    this->owner = owner;
    keyToMap = key;

    if ( isKeyboard() )
        KeyboardManager::get().hook ( this, window );
}

void Controller::cancelMapping()
{
    KeyboardManager::get().unhook();

    owner = 0;
    keyToMap = 0;

    for ( auto& a : activeMappings )
        for ( auto& b : a )
            for ( auto& c : b )
                c = 0;
}

void Controller::clearMapping ( uint32_t keys )
{
    for ( size_t i = 0; i < 32; ++i )
        if ( keys & ( 1u << i ) )
            keyboard[i] = 0;

    for ( auto& a : mappings )
        for ( auto& b : a )
            for ( auto& c : b )
                if ( c & keys )
                    c = 0;
}

inline static bool isPowerOfTwo ( uint32_t x )
{
    return ( x != 0 ) && ( ( x & ( x - 1 ) ) == 0 );
}

bool Controller::isOnlyGuid() const
{
    return isPowerOfTwo ( guidBitset[this->guid.guid] );
}

unordered_map<Guid, uint32_t> Controller::guidBitset;
