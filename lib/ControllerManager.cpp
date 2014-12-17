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


static HRESULT comInitRet = E_FAIL;

static IDirectInput8 *dinput = 0;


void ControllerManager::check ( void *keyboardWindowHandle )
{
    if ( !initialized )
        return;

    if ( !keyboardWindowHandle || keyboardWindowHandle == ( void * ) GetForegroundWindow() )
    {
        // Update keyboard controller state
        keyboard.prevState = keyboard.state;
        keyboard.state = 0;

        for ( uint8_t i = 0; i < 32; ++i )
        {
            if ( !keyboard.keybd.codes[i] )
                continue;

            if ( GetKeyState ( keyboard.keybd.codes[i] ) & 0x80 )
                keyboard.state |= ( 1u << i );
        }
    }

    for ( auto& kv : joysticks )
    {
        kv.second->prevAnyButton = kv.second->anyButton;
        kv.second->prevState = kv.second->state;
    }

    // while ( al_get_next_event ( eventQueue, &event ) )
    // {
    //     switch ( event.type )
    //     {
    //         case ALLEGRO_EVENT_JOYSTICK_AXIS:
    //         {
    //             Controller *controller = joysticks[ ( void * ) event.joystick.id ].get();

    //             ASSERT ( controller != 0 );

    //             LOG_CONTROLLER ( controller, "ALLEGRO_EVENT_JOYSTICK_AXIS; stick=%d; axis=%d; pos=%.2f",
    //                              event.joystick.stick, event.joystick.axis, event.joystick.pos );

    //             const uint8_t axis = combineAxis ( 'X' + event.joystick.axis, event.joystick.stick );

    //             uint8_t value = AXIS_CENTERED;
    //             if ( abs ( event.joystick.pos ) > controller->stick.deadzone )
    //                 value = ( event.joystick.pos > 0.0f ? AXIS_POSITIVE : AXIS_NEGATIVE );

    //             controller->joystickAxisEvent ( axis, value );
    //             break;
    //         }

    //         case ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN:
    //         {
    //             Controller *controller = joysticks[ ( void * ) event.joystick.id ].get();

    //             ASSERT ( controller != 0 );

    //             LOG_CONTROLLER ( controller, "ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN; button=%d", event.joystick.button );

    //             controller->joystickButtonEvent ( event.joystick.button, true );
    //             break;
    //         }

    //         case ALLEGRO_EVENT_JOYSTICK_BUTTON_UP:
    //         {
    //             Controller *controller = joysticks[ ( void * ) event.joystick.id ].get();

    //             ASSERT ( controller != 0 );

    //             LOG_CONTROLLER ( controller, "ALLEGRO_EVENT_JOYSTICK_BUTTON_UP; button=%d", event.joystick.button );

    //             controller->joystickButtonEvent ( event.joystick.button, false );
    //             break;
    //         }

    //         case ALLEGRO_EVENT_JOYSTICK_CONFIGURATION:
    //             LOG ( "ALLEGRO_EVENT_JOYSTICK_CONFIGURATION" );
    //             refreshJoysticks();
    //             break;

    //         default:
    //             LOG ( "Unknown event type (%d)", event.type );
    //             break;
    //     }
    // }
}

// Note: this is called on the SAME thread where IDirectInput8_EnumDevices is called (see below)
static BOOL CALLBACK enumJoysticks ( const DIDEVICEINSTANCE *ddi, void * )
{
    LOG ( "%s", ddi->tszProductName );

    return DIENUM_CONTINUE;
}

void ControllerManager::refreshJoysticks()
{
    if ( !initialized )
        return;

    HRESULT result = IDirectInput8_EnumDevices ( dinput, DI8DEVCLASS_GAMECTRL, enumJoysticks, 0, DIEDFL_ATTACHEDONLY );
    if ( result != DI_OK )
        THROW_EXCEPTION ( "IDirectInput8_EnumDevices failed: 0x%08x", ERROR_CONTROLLER_CHECK, result );

    // al_reconfigure_joysticks();

    // vector<const ALLEGRO_JOYSTICK *> dead;

    // for ( const auto& kv : joysticks )
    // {
    //     ALLEGRO_JOYSTICK *joystick = ( ALLEGRO_JOYSTICK * ) kv.first;
    //     Controller *controller = kv.second.get();

    //     if ( !al_get_joystick_active ( joystick ) )
    //     {
    //         dead.push_back ( joystick );
    //         joysticksByName.erase ( controller->getName() );

    //         LOG_CONTROLLER ( controller, "detached" );

    //         if ( owner )
    //             owner->detachedJoystick ( controller );
    //     }
    // }

    // for ( const ALLEGRO_JOYSTICK *joystick : dead )
    //     joysticks.erase ( ( void * ) joystick );

    // for ( int i = 0; i < al_get_num_joysticks(); ++i )
    // {
    //     void *joystick = ( void * ) al_get_joystick ( i );

    //     if ( joysticks.find ( joystick ) != joysticks.end() )
    //         continue;

    //     Controller *controller = new Controller ( joystick );

    //     ASSERT ( controller != 0 );

    //     joysticks[joystick].reset ( controller );
    //     joysticksByName[controller->getName()] = controller;

    //     auto it = mappings.mappings.find ( controller->getName() );
    //     if ( it != mappings.mappings.end() && it->second->getMsgType() == MsgType::JoystickMappings )
    //     {
    //         controller->setMappings ( it->second->getAs<JoystickMappings>() );
    //     }
    //     else
    //     {
    //         auto it = mappings.mappings.find ( controller->getOrigName() );
    //         if ( it != mappings.mappings.end() && it->second->getMsgType() == MsgType::JoystickMappings )
    //             controller->setMappings ( it->second->getAs<JoystickMappings>() );
    //     }

    //     LOG_CONTROLLER ( controller, "attached" );

    //     if ( owner )
    //         owner->attachedJoystick ( controller );
    // }
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
