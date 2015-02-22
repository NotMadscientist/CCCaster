#include "ControllerManager.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#define INITGUID
#define DIRECTINPUT_VERSION 0x0800 // Need at least version 8
#include <dinput.h>
#define COBJMACROS
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <map>

using namespace std;


#define JOYSTICK_FLAGS  ( DISCL_NONEXCLUSIVE | DISCL_BACKGROUND )

#define AXIS_MAX        ( 32767 )

#define AXIS_MIN        ( -32768 )


static HRESULT comInitRet = E_FAIL;

static IDirectInput8 *dinput = 0;


static inline uint8_t mapAxisValue ( LONG value, uint32_t deadzone )
{
    if ( abs ( value ) > ( LONG ) deadzone )
        return ( value > 0 ? AXIS_POSITIVE : AXIS_NEGATIVE );

    return AXIS_CENTERED;
}

static inline uint8_t mapHatValue ( uint32_t value )
{
    static const uint8_t values[] =
    {
        8, // Up
        9, // Up-right
        6, // Right
        3, // Down-right
        2, // Down
        1, // Down-left
        4, // Left
        7, // Up-left
    };

    if ( LOWORD ( value ) == 0xFFFF )
        return 5;

    // The original value has range [0,36000), starting with 0 as north, and rotating clockwise.
    // So we need to round the value down to the range [0, 8).
    value %= 36000;
    value /= 4500;

    ASSERT ( value < 8 );

    return values[value];
}


void ControllerManager::savePrevStates()
{
    LOCK ( mutex );

    if ( !initialized )
        return;

    if ( windowHandle == ( void * ) GetForegroundWindow() )
    {
        keyboard.prevState = keyboard.state;
    }

    for ( auto& kv : joysticks )
    {
        Controller *controller = kv.second.get();
        controller->prevState = controller->state;
    }
}

bool ControllerManager::check()
{
    LOCK ( mutex );

    if ( !initialized )
        return false;

    if ( windowHandle == ( void * ) GetForegroundWindow() )
    {
        // Update keyboard controller state
        keyboard.state = 0;

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( !keyboard.keyboardMappings.codes[i] )
                continue;

            if ( GetKeyState ( keyboard.keyboardMappings.codes[i] ) & 0x80 )
                keyboard.state |= ( 1u << i );
        }
    }

    DIJOYSTATE2 djs;
    HRESULT result;

    for ( auto it = joysticks.begin(); it != joysticks.end(); )
    {
        Controller *controller = it->second.get();
        const JoystickInfo info = controller->joystick.info;
        IDirectInputDevice8 *const device = ( IDirectInputDevice8 * ) info.device;
        const uint32_t deadzone = controller->joystickMappings.deadzone;

        // Save previous joystick states to generate regular controller events.
        // Note: this is independent of the prevState property, which is used to detect edge events per frame.
        controller->joystick.prevState = controller->joystick.state;

        // Poll device state
        result = IDirectInputDevice8_Poll ( device );
        if ( result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED )
        {
            IDirectInputDevice8_Acquire ( device );
            result = IDirectInputDevice8_Poll ( device );
        }

        if ( FAILED ( result ) )
        {
            LOG_CONTROLLER ( controller, "IDirectInputDevice8_Poll failed: 0x%08x", result );
            LOG ( "Detaching joystick: %s", it->first );
            detachJoystick ( ( it++ )->first );
            continue;
        }

        // Get device state
        result = IDirectInputDevice8_GetDeviceState ( device, sizeof ( DIJOYSTATE2 ), &djs );
        if ( result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED )
        {
            IDirectInputDevice8_Acquire ( device );
            result = IDirectInputDevice8_GetDeviceState ( device, sizeof ( DIJOYSTATE2 ), &djs );
        }

        if ( FAILED ( result ) )
        {
            LOG_CONTROLLER ( controller, "IDirectInputDevice8_GetDeviceState failed: 0x%08x", result );
            LOG ( "Detaching joystick: %s", it->first );
            detachJoystick ( ( it++ )->first );
            continue;
        }

        const uint8_t axisMask = info.axisMask;
        uint8_t *const axes = controller->joystick.state.axes;
        uint8_t axis = 0;

        if ( axisMask & 0x01u )
            axes[axis++] = mapAxisValue ( djs.lX, deadzone );
        if ( axisMask & 0x02u )
            axes[axis++] = mapAxisValue ( -djs.lY, deadzone ); // DirectInput, like most APIs, reports inverted Y-axis
        if ( axisMask & 0x04u )
            axes[axis++] = mapAxisValue ( djs.lZ, deadzone );
        if ( axisMask & 0x08u )
            axes[axis++] = mapAxisValue ( djs.lRx, deadzone );
        if ( axisMask & 0x10u )
            axes[axis++] = mapAxisValue ( -djs.lRy, deadzone ); // DirectInput, like most APIs, reports inverted Y-axis
        if ( axisMask & 0x20u )
            axes[axis++] = mapAxisValue ( djs.lRz, deadzone );
        if ( axisMask & 0x40u )
            axes[axis++] = mapAxisValue ( djs.rglSlider[0], deadzone );
        if ( axisMask & 0x80u )
            axes[axis++] = mapAxisValue ( djs.rglSlider[1], deadzone );

        ASSERT ( axis == info.numAxes );

        uint8_t *const prevAxes = controller->joystick.prevState.axes;

        for ( axis = 0; axis < info.numAxes; ++axis )
        {
            if ( axes[axis] == prevAxes[axis] )
                continue;

            LOG_CONTROLLER ( controller, "axis%u: %u -> %u", axis, prevAxes[axis], axes[axis] );
            controller->joystickAxisEvent ( axis, axes[axis] );
        }

        uint8_t *const hats = controller->joystick.state.hats;
        uint8_t *const prevHats = controller->joystick.prevState.hats;

        for ( uint8_t hat = 0; hat < info.numHats; ++hat )
        {
            hats[hat] = mapHatValue ( djs.rgdwPOV[hat] );

            if ( hats[hat] == prevHats[hat] )
                continue;

            LOG_CONTROLLER ( controller, "hat%u: %u -> %u", hat, prevHats[hat], hats[hat] );
            controller->joystickHatEvent ( hat, hats[hat] );
        }

        uint32_t& buttons = controller->joystick.state.buttons;
        buttons = 0;

        const uint32_t prevButtons = controller->joystick.prevState.buttons;

        for ( uint8_t button = 0; button < info.numButtons; ++button )
        {
            const uint8_t value = ( djs.rgbButtons[button] ? 1 : 0 );
            buttons |= ( value << button );

            if ( ( buttons & ( 1u << button ) ) == ( prevButtons & ( 1u << button ) ) )
                continue;

            LOG_CONTROLLER ( controller, "button%u: %s", button, ( value ? "0 -> 1" : "1 -> 0" ) );
            controller->joystickButtonEvent ( button, value );
        }

        ++it;
    }

    return true;
}

static BOOL CALLBACK enumJoystickAxes ( const DIDEVICEOBJECTINSTANCE *ddoi, void *userPtr )
{
    if ( ! ( ddoi->dwType & DIDFT_AXIS ) )
        return DIENUM_CONTINUE;

    JoystickInfo& info = * ( JoystickInfo * ) userPtr;

    if ( info.numAxes >= MAX_NUM_AXES )
        return DIENUM_CONTINUE;

    bool added = false;

#define CHECK_ADD_AXIS(INDEX, GUID_CONSTANT, NAME)                                                  \
    do {                                                                                            \
        if ( added )                                                                                \
            break;                                                                                  \
        if ( info.axisMask & ( 1u << INDEX ) )                                                      \
            break;                                                                                  \
        if ( Guid ( ddoi->guidType ) == Guid ( GUID_CONSTANT ) ) {                                  \
            ++info.numAxes;                                                                         \
            if ( info.axisNames.size() < INDEX + 1 )                                                \
                info.axisNames.resize ( INDEX + 1 );                                                \
            info.axisNames[INDEX] = NAME;                                                           \
            info.axisMask |= ( 1u << INDEX );                                                       \
            added = true;                                                                           \
        }                                                                                           \
    } while ( 0 )

    CHECK_ADD_AXIS ( 0, GUID_XAxis, "X-Axis" );
    CHECK_ADD_AXIS ( 1, GUID_YAxis, "Y-Axis" );
    CHECK_ADD_AXIS ( 2, GUID_ZAxis, "Z-Axis" );
    CHECK_ADD_AXIS ( 3, GUID_RxAxis, "X-Axis (2)" );
    CHECK_ADD_AXIS ( 4, GUID_RyAxis, "Y-Axis (2)" );
    CHECK_ADD_AXIS ( 5, GUID_RzAxis, "Z-Axis (2)" );
    CHECK_ADD_AXIS ( 6, GUID_Slider, "Slider" );
    CHECK_ADD_AXIS ( 7, GUID_Slider, "Slider (2)" );

    if ( !added )
        return DIENUM_CONTINUE;

    HRESULT result;

    // Set maximum range
    DIPROPRANGE range;
    range.diph.dwSize = sizeof ( range );
    range.diph.dwHeaderSize = sizeof ( range.diph );
    range.diph.dwObj = ddoi->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = AXIS_MIN;
    range.lMax = AXIS_MAX;

    result = IDirectInputDevice8_SetProperty ( ( IDirectInputDevice8 * ) info.device, DIPROP_RANGE, &range.diph );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_SetProperty DIPROP_RANGE failed: 0x%08x", result );

    // Set 0 deadzone
    DIPROPDWORD deadzone;
    deadzone.diph.dwSize = sizeof ( deadzone );
    deadzone.diph.dwHeaderSize = sizeof ( deadzone.diph );
    deadzone.diph.dwObj = ddoi->dwType;
    deadzone.diph.dwHow = DIPH_BYID;
    deadzone.dwData = 0;

    result = IDirectInputDevice8_SetProperty ( ( IDirectInputDevice8 * ) info.device, DIPROP_DEADZONE, &deadzone.diph );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_SetProperty DIPROP_DEADZONE failed: 0x%08x", result );

    return DIENUM_CONTINUE;
}

void ControllerManager::attachJoystick ( const Guid& guid, const string& name )
{
    if ( !initialized )
        return;

    IDirectInputDevice8 *tmp, *device;

    GUID windowsGuid;
    guid.getGUID ( windowsGuid );

    // Create the DirectInput device, NOTE the pointer returned is only temporarily used
    HRESULT result = IDirectInput8_CreateDevice ( dinput, windowsGuid, &tmp, 0 );
    if ( FAILED ( result ) )
    {
        LOG ( "IDirectInput8_CreateDevice failed: 0x%08x", result );
        return;
    }

    // Query for the actual joystick device to use
    result = IDirectInputDevice8_QueryInterface ( tmp, IID_IDirectInputDevice8, ( void ** ) &device );

    // We no longer need the original DirectInput device pointer
    IDirectInputDevice8_Release ( tmp );

    if ( FAILED ( result ) )
    {
        LOG ( "IDirectInputDevice8_QueryInterface failed: 0x%08x", result );
        return;
    }

    // Enable shared background access
    result = IDirectInputDevice8_SetCooperativeLevel ( device, ( HWND ) windowHandle, JOYSTICK_FLAGS );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_SetCooperativeLevel failed: 0x%08x", result ); // Non-fatal

    // Use the DIJOYSTATE2 data format
    result = IDirectInputDevice8_SetDataFormat ( device, &c_dfDIJoystick2 );
    if ( FAILED ( result ) )
    {
        LOG ( "IDirectInputDevice8_SetDataFormat failed: 0x%08x", result );
        return;
    }

    DIDEVCAPS ddc;
    ddc.dwSize = sizeof ( ddc );

    // Get the joystick capabilities
    result = IDirectInputDevice8_GetCapabilities ( device, &ddc );
    if ( FAILED ( result ) )
    {
        LOG ( "IDirectInputDevice8_GetCapabilities failed: 0x%08x", result );
        return;
    }

    JoystickInfo info;
    info.device = device;
    info.numHats = min<uint64_t> ( MAX_NUM_HATS, ddc.dwPOVs );
    info.numButtons = min<uint64_t> ( MAX_NUM_BUTTONS, ddc.dwButtons );

    // Initialize the properties for each axis
    result = IDirectInputDevice8_EnumObjects ( device, enumJoystickAxes, &info, DIDFT_AXIS );
    if ( FAILED ( result ) )
    {
        LOG ( "IDirectInputDevice8_EnumObjects failed: 0x%08x", result );
        return;
    }

    // Create and add the controller
    Controller *controller = new Controller ( name, info );

    ASSERT ( controller != 0 );

    joysticks[guid].reset ( controller );
    joysticksByName[controller->getName()] = controller;

    // Update mappings
    auto it = mappings.mappings.find ( controller->getName() );
    if ( it != mappings.mappings.end() && it->second->getMsgType() == MsgType::JoystickMappings )
    {
        controller->setMappings ( it->second->getAs<JoystickMappings>() );
    }
    else
    {
        auto it = mappings.mappings.find ( controller->getOrigName() );
        if ( it != mappings.mappings.end() && it->second->getMsgType() == MsgType::JoystickMappings )
            controller->setMappings ( it->second->getAs<JoystickMappings>() );
    }

    LOG_CONTROLLER ( controller, "attached" );

    if ( owner )
        owner->attachedJoystick ( controller );
}

void ControllerManager::detachJoystick ( const Guid& guid )
{
    if ( !initialized )
        return;

    // Find and remove the controller
    auto it = joysticks.find ( guid );

    ASSERT ( it != joysticks.end() );

    Controller *controller = it->second.get();
    IDirectInputDevice8 *const device = ( IDirectInputDevice8 * ) controller->joystick.info.device;

    LOG_CONTROLLER ( controller, "detached" );

    if ( owner )
        owner->detachedJoystick ( controller );

    joysticksByName.erase ( controller->getName() );
    joysticks.erase ( it );

    // Release DirectInput joystick device
    IDirectInputDevice8_Unacquire ( device );
    IDirectInputDevice8_Release ( device );
}

static BOOL CALLBACK enumJoysticks ( const DIDEVICEINSTANCE *ddi, void *userPtr )
{
    unordered_map<Guid, string>& activeJoysticks = * ( unordered_map<Guid, string> * ) userPtr;

    const Guid guid = ddi->guidInstance;

    LOG ( "%s: guid='%s'", ddi->tszProductName, guid );

    activeJoysticks[guid] = ddi->tszProductName;

    return DIENUM_CONTINUE;
}

void ControllerManager::refreshJoysticks()
{
    LOCK ( mutex );

    if ( !initialized )
        return;

    unordered_map<Guid, string> activeJoysticks;

    HRESULT result = IDirectInput8_EnumDevices ( dinput, DI8DEVCLASS_GAMECTRL,
                     enumJoysticks, ( void * ) &activeJoysticks, DIEDFL_ATTACHEDONLY );

    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInput8_EnumDevices failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    for ( const auto& kv : activeJoysticks )
    {
        if ( joysticks.find ( kv.first ) != joysticks.end() )
            continue;

        LOG ( "Attaching joystick: %s", kv.first );
        attachJoystick ( kv.first, kv.second );
    }

    for ( auto it = joysticks.begin(); it != joysticks.end(); )
    {
        if ( activeJoysticks.find ( it->first ) != activeJoysticks.end() )
        {
            ++it;
            continue;
        }

        LOG ( "Detaching joystick: %s", it->first );
        detachJoystick ( ( it++ )->first );
    }
}

void ControllerManager::initialize ( Owner *owner )
{
    LOCK ( mutex );

    if ( initialized )
        return;

    initialized = true;

    this->owner = owner;

    comInitRet = CoInitializeEx ( 0, COINIT_APARTMENTTHREADED );
    if ( FAILED ( comInitRet ) )
        comInitRet = CoInitializeEx ( 0, COINIT_MULTITHREADED );
    if ( FAILED ( comInitRet ) )
        THROW_EXCEPTION ( "CoInitializeEx failed: 0x%08x", ERROR_CONTROLLER_INIT, comInitRet );

    HRESULT result = CoCreateInstance ( CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER,
                                        IID_IDirectInput8, ( void ** ) &dinput );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "CoCreateInstance failed: 0x%08x", ERROR_CONTROLLER_INIT, result );

    result = IDirectInput8_Initialize ( dinput, GetModuleHandle ( 0 ), DIRECTINPUT_VERSION );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInput8_Initialize failed: 0x%08x", ERROR_CONTROLLER_INIT, result );

    refreshJoysticks();

    LOG ( "ControllerManager initialized" );
}

void ControllerManager::deinitialize()
{
    {
        LOCK ( mutex );

        if ( !initialized )
            return;

        initialized = false;

        this->owner = 0;
    }

    pollingThread.join();

    ControllerManager::get().clear();

    if ( dinput )
    {
        IDirectInput8_Release ( dinput );
        dinput = 0;
    }

    if ( SUCCEEDED ( comInitRet ) )
    {
        CoUninitialize();
        comInitRet = E_FAIL;
    }

    LOG ( "ControllerManager deinitialized" );
}

void ControllerManager::mappingsChanged ( Controller *controller )
{
    LOCK ( mutex );

    LOG_CONTROLLER ( controller, "mappingsChanged" );

    mappings.mappings[controller->getName()] = controller->getMappings();
    mappings.invalidate();
}

void ControllerManager::clear()
{
    LOCK ( mutex );

    LOG ( "Clearing controllers" );

    if ( owner )
    {
        for ( auto& kv : joysticks )
            owner->detachedJoystick ( kv.second.get() );
    }

    joysticks.clear();
    joysticksByName.clear();
}

static bool compareControllerName ( const Controller *a, const Controller *b )
{
    return a->getName() < b->getName();
}

vector<Controller *> ControllerManager::getJoysticks()
{
    LOCK ( mutex );

    vector<Controller *> controllers;
    controllers.reserve ( joysticks.size() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin(), controllers.end(), compareControllerName );
    return controllers;
}

vector<const Controller *> ControllerManager::getJoysticks() const
{
    LOCK ( mutex );

    vector<const Controller *> controllers;
    controllers.reserve ( joysticks.size() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin(), controllers.end(), compareControllerName );
    return controllers;
}

vector<Controller *> ControllerManager::getControllers()
{
    LOCK ( mutex );

    vector<Controller *> controllers;
    controllers.reserve ( joysticks.size() + 1 );

    controllers.push_back ( getKeyboard() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin() + 1, controllers.end(), compareControllerName );
    return controllers;
}

vector<const Controller *> ControllerManager::getControllers() const
{
    LOCK ( mutex );

    vector<const Controller *> controllers;
    controllers.reserve ( joysticks.size() + 1 );

    controllers.push_back ( getKeyboard() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin() + 1, controllers.end(), compareControllerName );
    return controllers;
}

ControllerManager::ControllerManager() : keyboard ( Controller::Keyboard ) {}

ControllerManager& ControllerManager::get()
{
    static ControllerManager instance;
    return instance;
}

void ControllerMappings::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( mappings.size() );

    for ( const auto& kv : mappings )
        ar ( kv.first, Protocol::encode ( kv.second ) );
}

void ControllerMappings::load ( cereal::BinaryInputArchive& ar )
{
    size_t count;
    ar ( count );

    string name;
    string buffer;
    size_t consumed;

    for ( size_t i = 0; i < count; ++i )
    {
        ar ( name, buffer );

        mappings[name] = Protocol::decode ( &buffer[0], buffer.size(), consumed );

        ASSERT ( consumed == buffer.size() );
    }
}

size_t ControllerManager::saveMappings ( const string& folder, const string& ext ) const
{
    LOCK ( mutex );

    size_t count = 0;

    for ( auto& kv : mappings.mappings )
    {
        const string file = folder + kv.first + ext;

        if ( !saveMappings ( file, kv.second ) )
            continue;

        ++count;

        LOG ( "Saved: %s", file );
    }

    return count;
}

size_t ControllerManager::loadMappings ( const string& folder, const string& ext )
{
    LOCK ( mutex );

    WIN32_FIND_DATA fd;
    HANDLE handle = 0;

    // File path with glob
    string path = folder + "*" + ext;

    if ( ( handle = FindFirstFile ( path.c_str(), &fd ) ) == INVALID_HANDLE_VALUE )
    {
        LOG ( "Path not found: %s", path );
        return 0;
    }

    size_t count = 0;

    do
    {
        // Ignore "." and ".."
        if ( !strcmp ( fd.cFileName, "." ) || !strcmp ( fd.cFileName, ".." ) )
            continue;

        // Ignore folders
        if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            continue;

        MsgPtr msg = loadMappings ( folder + fd.cFileName );

        if ( !msg )
            continue;

        if ( msg->getMsgType() == MsgType::KeyboardMappings )
            mappings.mappings [ msg->getAs<KeyboardMappings>().name ] = msg;
        else if ( msg->getMsgType() == MsgType::JoystickMappings )
            mappings.mappings [ msg->getAs<JoystickMappings>().name ] = msg;
        else
            continue;

        ++count;

        LOG ( "Loaded: %s", folder + fd.cFileName );
    }
    while ( FindNextFile ( handle, &fd ) ); // Find the next file.

    FindClose ( handle );

    setMappings ( mappings );

    return count;
}

bool ControllerManager::saveMappings ( const string& file, const MsgPtr& mappings )
{
    if ( mappings->getMsgType() == MsgType::KeyboardMappings )
        return saveMappings ( file, mappings->getAs<KeyboardMappings>() );

    if ( mappings->getMsgType() == MsgType::JoystickMappings )
        return saveMappings ( file, mappings->getAs<JoystickMappings>() );

    return false;
}

bool ControllerManager::saveMappings ( const string& file, const KeyboardMappings& mappings )
{
    ofstream fout ( file.c_str(), ios::binary );
    bool good = fout.good();

    if ( good )
    {
        const string buffer = Protocol::encode ( mappings );

        fout.write ( &buffer[0], buffer.size() );

        good = fout.good();
    }

    fout.close();
    return good;
}

bool ControllerManager::saveMappings ( const string& file, const JoystickMappings& mappings )
{
    ofstream fout ( file.c_str(), ios::binary );
    bool good = fout.good();

    if ( good )
    {
        const string buffer = Protocol::encode ( mappings );

        fout.write ( &buffer[0], buffer.size() );

        good = fout.good();
    }

    fout.close();
    return good;
}

MsgPtr ControllerManager::loadMappings ( const string& file )
{
    MsgPtr msg;
    ifstream fin ( file.c_str(), ios::binary );
    bool good = fin.good();

    if ( good )
    {
        stringstream ss;
        ss << fin.rdbuf();

        string buffer = ss.str();
        size_t consumed;

        msg = Protocol::decode ( &buffer[0], buffer.size(), consumed );

        if ( !msg )
            LOG ( "Failed to decode %u bytes", buffer.size() );
        else if ( consumed != buffer.size() )
            LOG ( "Warning: consumed bytes %u != buffer size %u", consumed, buffer.size() );
    }

    fin.close();
    return msg;
}

void ControllerManager::PollingThread::run()
{
    while ( ControllerManager::get().check() )
    {
        Sleep ( 1 );
    }
}

void ControllerManager::startHighFreqPolling()
{
    pollingThread.start();
}
