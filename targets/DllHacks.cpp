#include "DllHacks.h"
#include "DllAsmHacks.h"
#include "D3DHook.h"
#include "Exceptions.h"
#include "ProcessManager.h"
#include "Algorithms.h"
#include "KeyboardManager.h"
#include "MouseManager.h"
#include "ControllerManager.h"
#include "DllFrameRate.h"

#define INITGUID
#include <windows.h>
#include <windowsx.h>
#include <dbt.h>
#include <MinHook.h>

using namespace std;
using namespace AsmHacks;
using namespace DllHacks;


DEFINE_GUID ( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 );

void stopDllMain ( const string& error );


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

    for ( const Asm& hack : detectRoundStart )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : filterRepeatedSfx )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : muteSpecificSfx )
        WRITE_ASM_HACK ( hack );

    WRITE_ASM_HACK ( detectAutoReplaySave );
    WRITE_ASM_HACK ( hijackEscapeKey );
    WRITE_ASM_HACK ( disableTrainingMusicReset );
    WRITE_ASM_HACK ( hijackCharaSelectColors );
}

// Note: this is called on the SAME thread as the main application thread
MH_WINAPI_HOOK ( LRESULT, CALLBACK, WindowProc, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
        case WM_SYSCOMMAND:
            // Eat these two events to prevent screensaver and sleep
            switch ( wParam )
            {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    return 0;

                default:
                    break;
            }
            break;

        case WM_KEYDOWN:
            // Ignore repeated keys
            if ( ( lParam >> 30 ) & 1 )
                break;

        // Intentional fall-through

        case WM_SYSKEYDOWN:
            // Handle Alt + F4
            if ( ( HIWORD ( lParam ) & KF_ALTDOWN ) && ( wParam == VK_F4 ) )
                stopDllMain ( "" );
            break;

        case WM_KEYUP:
        {
            // Only inject keyboard events when hooked
            if ( !KeyboardManager::get().isHooked() || !KeyboardManager::get().owner )
                break;

            const uint32_t vkCode = wParam;
            const uint32_t scanCode = ( lParam >> 16 ) & 127;
            const bool isExtended = ( lParam >> 24 ) & 1;
            const bool isDown = ( lParam >> 31 ) & 1;

            LOG ( "vkCode=0x%02X; scanCode=%u; isExtended=%u; isDown=%u", vkCode, scanCode, isExtended, isDown );

            // Note: this doesn't actually eat the keyboard event, which is actually acceptable
            // for the in-game overlay UI, since we need to mix usage with GetKeyState.
            KeyboardManager::get().owner->keyboardEvent ( vkCode, scanCode, isExtended, isDown );
            break;
        }

        case WM_DEVICECHANGE:
            switch ( wParam )
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
                    if ( ( ( DEV_BROADCAST_HDR * ) lParam )->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
                    {
                        ControllerManager::get().refreshJoysticks();
                    }
                    break;
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        {
            // Only inject mouse events when hooked
            if ( !MouseManager::get().owner )
                break;

            static bool isDown = false;

            if ( msg == WM_LBUTTONDOWN )
                isDown = true;
            else if ( msg ==  WM_LBUTTONUP )
                isDown = false;

            int x = GET_X_LPARAM ( lParam );
            int y = GET_Y_LPARAM ( lParam );

            MouseManager::get().owner->mouseEvent ( x, y, isDown, ( msg == WM_LBUTTONDOWN ), ( msg == WM_LBUTTONUP ) );
            break;
        }

        default:
            break;
    }

    return oWindowProc ( hwnd, msg, wParam, lParam );
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

    // Apparently this needs to be applied AFTER the game loads
    DllFrameRate::enable();

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


// The following constructors should only be called when running in the DLL, ie MBAA's memory space
InitialGameState::InitialGameState ( IndexedFrame indexedFrame, uint8_t netplayState, bool isTraining )
    : indexedFrame ( indexedFrame )
    , stage ( *CC_STAGE_SELECTOR_ADDR )
    , netplayState ( netplayState )
    , isTraining ( isTraining )
{
    chara[0] = ( uint8_t ) * CC_P1_CHARACTER_ADDR;
    chara[1] = ( uint8_t ) * CC_P2_CHARACTER_ADDR;

    moon[0] = ( uint8_t ) * CC_P1_MOON_SELECTOR_ADDR;
    moon[1] = ( uint8_t ) * CC_P2_MOON_SELECTOR_ADDR;

    color[0] = ( uint8_t ) * CC_P1_COLOR_SELECTOR_ADDR;
    color[1] = ( uint8_t ) * CC_P2_COLOR_SELECTOR_ADDR;
}

SyncHash::SyncHash ( IndexedFrame indexedFrame )
{
    this->indexedFrame = indexedFrame;

    char data [ sizeof ( uint32_t ) * 3 + CC_RNG_STATE3_SIZE ];

    memcpy ( &data[0], CC_RNG_STATE0_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[4], CC_RNG_STATE1_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[8], CC_RNG_STATE2_ADDR, sizeof ( uint32_t ) );
    memcpy ( &data[12], CC_RNG_STATE3_ADDR, CC_RNG_STATE3_SIZE );

    getMD5 ( data, sizeof ( data ), hash );

    if ( *CC_GAME_MODE_ADDR != CC_GAME_MODE_IN_GAME )
    {
        memset ( &chara[0], 0, sizeof ( CharaHash ) );
        memset ( &chara[1], 0, sizeof ( CharaHash ) );
        chara[0].chara = ( uint16_t ) * CC_P1_CHARACTER_ADDR;
        chara[0].moon  = ( uint16_t ) * CC_P1_MOON_SELECTOR_ADDR;
        chara[1].chara = ( uint16_t ) * CC_P2_CHARACTER_ADDR;
        chara[1].moon  = ( uint16_t ) * CC_P2_MOON_SELECTOR_ADDR;
        return;
    }

    roundTimer = *CC_ROUND_TIMER_ADDR;
    realTimer = *CC_REAL_TIMER_ADDR;
    cameraX = *CC_CAMERA_X_ADDR;
    cameraY = *CC_CAMERA_Y_ADDR;

#define SAVE_CHARA(N)                                                                           \
    chara[N-1].seq          = *CC_P ## N ## _SEQUENCE_ADDR;                                     \
    chara[N-1].seqState     = *CC_P ## N ## _SEQ_STATE_ADDR;                                    \
    chara[N-1].health       = *CC_P ## N ## _HEALTH_ADDR;                                       \
    chara[N-1].redHealth    = *CC_P ## N ## _RED_HEALTH_ADDR;                                   \
    chara[N-1].meter        = *CC_P ## N ## _METER_ADDR;                                        \
    chara[N-1].heat         = *CC_P ## N ## _HEAT_ADDR;                                         \
    chara[N-1].guardBar     = ( *CC_INTRO_STATE_ADDR ? 0 : *CC_P ## N ## _GUARD_BAR_ADDR );     \
    chara[N-1].guardQuality = *CC_P ## N ## _GUARD_QUALITY_ADDR;                                \
    chara[N-1].x            = *CC_P ## N ## _X_POSITION_ADDR;                                   \
    chara[N-1].y            = *CC_P ## N ## _Y_POSITION_ADDR;                                   \
    chara[N-1].chara = ( uint16_t ) * CC_P ## N ## _CHARACTER_ADDR;                             \
    chara[N-1].moon  = ( uint16_t ) * CC_P ## N ## _MOON_SELECTOR_ADDR;

    SAVE_CHARA ( 1 )
    SAVE_CHARA ( 2 )
}
