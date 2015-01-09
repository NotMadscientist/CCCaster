#include "DllHacks.h"
#include "AsmHacks.h"
#include "D3DHook.h"
#include "Exceptions.h"
#include "ProcessManager.h"
#include "Algorithms.h"
#include "KeyboardManager.h"
#include "ControllerManager.h"
#include "DllFrameRate.h"

#define INITGUID
#include <windows.h>
#include <dbt.h>
#include <MinHook.h>

using namespace std;
using namespace AsmHacks;
using namespace DllHacks;


DEFINE_GUID ( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 );


namespace DllHacks
{

void initializePreLoad()
{
    for ( const Asm& hack : hookMainLoop )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : hijackControls )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : hijackMenu )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : filterRepeatedSfx )
        WRITE_ASM_HACK ( hack );

    WRITE_ASM_HACK ( detectAutoReplaySave );
    WRITE_ASM_HACK ( hijackEscapeKey );

    DllFrameRate::enable();
}

// Note: this is called on the SAME thread as the main application thread
MH_WINAPI_HOOK ( LRESULT, CALLBACK, WindowProc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
        case WM_KEYDOWN:
            // Ignore repeated keys
            if ( ( lParam >> 30 ) & 1 )
                break;

        case WM_KEYUP:
        {
            // Only inject keyboard events when hooked
            if ( !KeyboardManager::get().isHooked() )
                break;

            const uint32_t vkCode = wParam;
            const uint32_t scanCode = ( lParam >> 16 ) & 127;
            const bool isExtended = ( lParam >> 24 ) & 1;
            const bool isDown = ( lParam >> 31 ) & 1;

            LOG ( "vkCode=0x%02X; scanCode=%u; isExtended=%u; isDown=%u", vkCode, scanCode, isExtended, isDown );

            // Note: this doesn't actually eat the keyboard event, which is actually acceptable
            // for the in-game overlay UI, since we need to mix usage with GetKeyState.
            if ( KeyboardManager::get().owner )
                KeyboardManager::get().owner->keyboardEvent ( vkCode, scanCode, isExtended, isDown );
            break;
        }

        case WM_DEVICECHANGE:
            switch ( wParam )
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
                    if ( ( ( DEV_BROADCAST_HDR * ) lParam )->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE )
                        break;

                    ControllerManager::get().refreshJoysticks();
                    break;
            }
            return 0;

        default:
            break;
    }

    return oWindowProc ( hwnd, message, wParam, lParam );
}


static pWindowProc WindowProc = ( pWindowProc ) CC_WINDOW_PROC_ADDR;

static HDEVNOTIFY notifyHandle = 0;


void *windowHandle = 0;


void initializePostLoad()
{
    LOG ( "threadId=%08x", GetCurrentThreadId() );

    // Apparently this needs to be applied AFTER the game loads
    for ( const Asm& hack : enableDisabledStages )
        WRITE_ASM_HACK ( hack );

    // Get the handle to the main window
    if ( ! ( windowHandle = ProcessManager::findWindow ( CC_TITLE ) ) )
        LOG ( "Couldn't find window '%s'", CC_TITLE );

    DEV_BROADCAST_DEVICEINTERFACE dbh;
    memset ( &dbh, 0, sizeof ( dbh ) );
    dbh.dbcc_size = sizeof ( dbh );
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

    // Register for device notifications
    if ( ! ( notifyHandle = RegisterDeviceNotification ( windowHandle, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE ) ) )
        LOG ( "RegisterDeviceNotification failed: %s", WinException::getLastError() );

    // Hook the game's WindowProc
    MH_STATUS status = MH_Initialize();
    if ( status != MH_OK )
        LOG ( "Initialize failed: %s", MH_StatusString ( status ) );

    status = MH_CREATE_HOOK ( WindowProc );
    if ( status != MH_OK )
        LOG ( "Create hook failed: %s", MH_StatusString ( status ) );

    status = MH_EnableHook ( ( void * ) WindowProc );
    if ( status != MH_OK )
        LOG ( "Enable hook failed: %s", MH_StatusString ( status ) );

    // We can't save replays on Wine because MBAA crashes even without us.
    // We can't hook DirectX calls on Wine (yet?).
    if ( ProcessManager::isWine() )
    {
        *CC_AUTO_REPLAY_SAVE_ADDR = 0;
        return;
    }

    // Hook the game's DirectX calls
    string err;
    if ( ! ( err = InitDirectX ( windowHandle ) ).empty() )
        LOG ( "InitDirectX failed: %s", err );
    else if ( ! ( err = HookDirectX() ).empty() )
        LOG ( "HookDirectX failed: %s", err );
}

void deinitialize()
{
    UnhookDirectX();

    if ( notifyHandle )
        UnregisterDeviceNotification ( notifyHandle );

    if ( WindowProc )
    {
        MH_DisableHook ( ( void * ) WindowProc );
        MH_REMOVE_HOOK ( WindowProc );
        MH_Uninitialize();
        WindowProc = 0;
    }

    for ( int i = hookMainLoop.size() - 1; i >= 0; --i )
        hookMainLoop[i].revert();
}

} // namespace DllHacks
