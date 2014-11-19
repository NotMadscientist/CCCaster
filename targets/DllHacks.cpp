#include "AsmHacks.h"
#include "D3DHook.h"
#include "Logger.h"

#include <windows.h>
#include <d3dx9.h>
#include <MinHook.h>

using namespace std;
using namespace AsmHacks;


#define WRITE_ASM_HACK(ASM_HACK)                                                                \
    do {                                                                                        \
        WindowsException err;                                                                   \
        if ( ( err = ASM_HACK.write() ).code != 0 ) {                                           \
            LOG ( "%s; %s failed; addr=%08x", err, #ASM_HACK, ASM_HACK.addr );                  \
            exit ( 0 );                                                                         \
        }                                                                                       \
    } while ( 0 )


static ID3DXFont *font = 0;

static LRESULT CALLBACK keyboardCallback ( int, WPARAM, LPARAM );

static HHOOK keybdHook = 0;

string overlayText;

void *mainWindowHandle = 0;


// Note: this is on the SAME thread as the main thread where callback happens
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    if ( !font )
    {
        D3DXCreateFont (
            device,                         // D3D device pointer
            24,                             // height
            0,                              // width
            FW_BOLD,                        // weight
            1,                              // # of mipmap levels
            FALSE,                          // italic
            DEFAULT_CHARSET,                // charset
            OUT_DEFAULT_PRECIS,             // output precision
            ANTIALIASED_QUALITY,            // quality
            DEFAULT_PITCH | FF_DONTCARE,    // pitch and family
            "Courier New",                  // typeface name
            &font );
    }

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // This should be the only viewport with the same width as the main viewport
    if ( viewport.Width == * CC_SCREEN_WIDTH_ADDR )
    {
        const long centerX = ( long ) viewport.Width / 2;
        // const long centerY = ( long ) viewport.Height / 2;

        RECT rect;
        rect.left    = centerX - 200;
        rect.right   = centerX + 200;
        rect.top     = 0;
        rect.bottom  = 20;

        font->DrawText (
            0,                              // Text as a ID3DXSprite object
            &overlayText[0],                // Text as a C-string
            overlayText.size(),             // Number of letters, -1 for null-terminated
            &rect,                          // Text bounding RECT
            DT_CENTER,                      // Text formatting
            D3DCOLOR_XRGB ( 255, 0, 0 ) );  // Text colour
    }
}

void PresentFrameEnd ( IDirect3DDevice9 *device )
{
}

void InvalidateDeviceObjects()
{
    if ( font )
    {
        font->OnLostDevice();
        font = 0;
    }
}

void initializePreLoadHacks()
{
    for ( const Asm& hack : hookMainLoop )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : enableDisabledStages )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : hijackControls )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : hijackMenu )
        WRITE_ASM_HACK ( hack );

    for ( const Asm& hack : detectRoundStart )
        WRITE_ASM_HACK ( hack );

    WRITE_ASM_HACK ( detectAutoReplaySave );

    // TODO find an alternative because this doesn't work on Wine
    // WRITE_ASM_HACK ( disableFpsLimit );
}

void initializePostLoadHacks()
{
    *CC_DAMAGE_LEVEL_ADDR = 2;
    *CC_TIMER_SPEED_ADDR = 2;

    // *CC_DAMAGE_LEVEL_ADDR = 4;

    // Hook and ignore keyboard messages to prevent lag from unhandled messages
    if ( ! ( keybdHook = SetWindowsHookEx ( WH_KEYBOARD, keyboardCallback, 0, GetCurrentThreadId() ) ) )
        LOG ( "SetWindowsHookEx failed: %s", WindowsException ( GetLastError() ) );

    // Get the handle to the main window
    if ( ( mainWindowHandle = enumFindWindow ( CC_TITLE ) ) == 0 )
        LOG ( "Couldn't find window '%s'", CC_TITLE );

    if ( detectWine() )
        return;

    // Hook DirectX
    string err;
    if ( ! ( err = InitDirectX ( mainWindowHandle ) ).empty() )
        LOG ( "InitDirectX failed: %s", err );
    else if ( ! ( err = HookDirectX() ).empty() )
        LOG ( "HookDirectX failed: %s", err );
}

void deinitializeHacks()
{
    UnhookDirectX();

    if ( keybdHook )
        UnhookWindowsHookEx ( keybdHook );

    for ( int i = hookMainLoop.size() - 1; i >= 0; --i )
        hookMainLoop[i].revert();
}

LRESULT CALLBACK keyboardCallback ( int code, WPARAM wParam, LPARAM lParam )
{
    // Pass through the Alt and Enter keys
    if ( code == HC_ACTION && ( wParam == VK_MENU || wParam == VK_RETURN ) )
        return CallNextHookEx ( 0, code, wParam, lParam );

    return 1;
}
