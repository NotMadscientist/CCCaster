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


#define JOYSTICK_FLAGS ( DISCL_NONEXCLUSIVE | DISCL_BACKGROUND )

static HRESULT comInitRet = E_FAIL;

static IDirectInput8 *dinput = 0;


void ControllerManager::check()
{
    if ( !initialized )
        return;

    if ( windowHandle == ( void * ) GetForegroundWindow() )
    {
        // Update keyboard controller state
        keyboard.prevState = keyboard.state;
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

    for ( auto& kv : joysticks )
    {
        Controller *controller = kv.second.get();
        IDirectInputDevice8 *joystick = ( IDirectInputDevice8 * ) controller->joystick;

        // Save previous states
        controller->joystickPrevState = controller->joystickState;
        controller->prevAnyButton = controller->anyButton;
        controller->prevState = controller->state;

        // Poll device state
        result = IDirectInputDevice8_Poll ( joystick );
        if ( result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED )
        {
            IDirectInputDevice8_Acquire ( joystick );
            result = IDirectInputDevice8_Poll ( joystick );
        }

        if ( FAILED ( result ) )
        {
            LOG ( "IDirectInputDevice8_Poll failed: 0x%08x", result );
            controller->anyButton = 0;
            controller->state = 0;
            continue;
        }

        // Get device state
        result = IDirectInputDevice8_GetDeviceState ( joystick, sizeof ( DIJOYSTATE2 ), &djs );
        if ( result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED )
        {
            IDirectInputDevice8_Acquire ( joystick );
            result = IDirectInputDevice8_GetDeviceState ( joystick, sizeof ( DIJOYSTATE2 ), &djs );
        }

        if ( FAILED ( result ) )
        {
            LOG ( "IDirectInputDevice8_GetDeviceState failed: 0x%08x", result );
            controller->anyButton = 0;
            controller->state = 0;
            continue;
        }

        LOG ( "lX=%d; lY=%d; lZ=%d; lRx=%d; lRy=%d; lRz=%d; slider0=%d; slider1=%d; pov0=%d; pov3=%d; pov2=%d; pov3=%d",
              djs.lX, djs.lY, djs.lZ, djs.lRx, djs.lRy, djs.lRz, djs.rglSlider[0], djs.rglSlider[1],
              djs.rgdwPOV[0], djs.rgdwPOV[1], djs.rgdwPOV[2], djs.rgdwPOV[3] );

        string buttons;
        for ( uint32_t i = 0; i < controller->numButtons; ++i )
            buttons += ( i ? "; " : "" ) + format ( "b%d=%d", i, ( bool ) djs.rgbButtons[i] );

        LOG ( "%s", buttons );
    }
}

static BOOL CALLBACK enumJoystickAxes ( const DIDEVICEOBJECTINSTANCE *ddoi, void *userPtr )
{
    if ( ! ( ddoi->dwType & DIDFT_AXIS ) )
        return DIENUM_CONTINUE;

    // Set maximum range
    DIPROPRANGE range;
    range.diph.dwSize = sizeof ( range );
    range.diph.dwHeaderSize = sizeof ( range.diph );
    range.diph.dwObj = ddoi->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = -32768;
    range.lMax = 32767;

    HRESULT result = IDirectInputDevice8_SetProperty ( ( IDirectInputDevice8 * ) userPtr, DIPROP_RANGE, &range.diph );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_SetProperty DIPROP_RANGE failed: 0x%08x", result );

    // Set 0 deadzone
    DIPROPDWORD deadzone;
    deadzone.diph.dwSize = sizeof ( deadzone );
    deadzone.diph.dwHeaderSize = sizeof ( deadzone.diph );
    deadzone.diph.dwObj = ddoi->dwType;
    deadzone.diph.dwHow = DIPH_BYID;
    deadzone.dwData = 0;

    result = IDirectInputDevice8_SetProperty ( ( IDirectInputDevice8 * ) userPtr, DIPROP_DEADZONE, &deadzone.diph );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_SetProperty DIPROP_DEADZONE failed: 0x%08x", result );

    return DIENUM_CONTINUE;
}

void ControllerManager::attachJoystick ( const Guid& guid, const string& name )
{
    if ( !initialized )
        return;

    IDirectInputDevice8 *device, *joystick;

    GUID windowsGuid;
    guid.getGUID ( windowsGuid );

    // Create the DirectInput device, NOTE the pointer returned is only temporarily used
    HRESULT result = IDirectInput8_CreateDevice ( dinput, windowsGuid, &device, 0 );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInput8_CreateDevice failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    // Query for the actual joystick device to use
    result = IDirectInputDevice8_QueryInterface ( device, IID_IDirectInputDevice8, ( void ** ) &joystick );

    // We no longer need the original DirectInput device pointer
    IDirectInputDevice8_Release ( device );

    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInputDevice8_QueryInterface failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    // Enable shared background access
    result = IDirectInputDevice8_SetCooperativeLevel ( joystick, ( HWND ) windowHandle, JOYSTICK_FLAGS );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInputDevice8_SetCooperativeLevel failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    // Use the DIJOYSTATE2 data format
    result = IDirectInputDevice8_SetDataFormat ( joystick, &c_dfDIJoystick2 );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInputDevice8_SetDataFormat failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    DIDEVCAPS ddc;
    ddc.dwSize = sizeof ( ddc );

    // Get the joystick capabilities
    result = IDirectInputDevice8_GetCapabilities ( joystick, &ddc );
    if ( FAILED ( result ) )
        THROW_EXCEPTION ( "IDirectInputDevice8_GetCapabilities failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    // Initialize the properties for each axis
    result = IDirectInputDevice8_EnumObjects ( joystick, enumJoystickAxes, joystick, DIDFT_AXIS );
    if ( FAILED ( result ) )
        LOG ( "IDirectInputDevice8_EnumObjects failed: 0x%08x", result );

    // Create and add the controller
    Controller *controller = new Controller ( name, ( void * ) joystick, ddc.dwAxes, ddc.dwPOVs, ddc.dwButtons );

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
    IDirectInputDevice8 *joystick = ( IDirectInputDevice8 * ) controller->joystick;

    LOG_CONTROLLER ( controller, "detached" );

    if ( owner )
        owner->detachedJoystick ( controller );

    joysticksByName.erase ( controller->getName() );
    joysticks.erase ( it );

    // Release DirectInput joystick device
    IDirectInputDevice8_Unacquire ( joystick );
    IDirectInputDevice8_Release ( joystick );
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

        attachJoystick ( kv.first, kv.second );
    }

    for ( const auto& kv : joysticks )
    {
        if ( activeJoysticks.find ( kv.first ) != activeJoysticks.end() )
            continue;

        detachJoystick ( kv.first );
    }
}

void ControllerManager::initialize ( Owner *owner )
{
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
    if ( !initialized )
        return;

    initialized = false;

    this->owner = 0;

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
    LOG_CONTROLLER ( controller, "mappingsChanged" );

    mappings.mappings[controller->getName()] = controller->getMappings();
    mappings.invalidate();
}

void ControllerManager::clear()
{
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
    vector<Controller *> controllers;
    controllers.reserve ( joysticks.size() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin(), controllers.end(), compareControllerName );
    return controllers;
}

vector<const Controller *> ControllerManager::getJoysticks() const
{
    vector<const Controller *> controllers;
    controllers.reserve ( joysticks.size() );

    for ( auto& kv : joysticks )
        controllers.push_back ( kv.second.get() );

    sort ( controllers.begin(), controllers.end(), compareControllerName );
    return controllers;
}

vector<Controller *> ControllerManager::getControllers()
{
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
