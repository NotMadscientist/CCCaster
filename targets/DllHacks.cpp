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


void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    if ( !font )
    {
        D3DXCreateFont (
            device,
            24,                             // height
            0,                              // width
            FW_NORMAL,                      // weight
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
    if ( viewport.Width == * ( uint32_t * ) CC_SCREEN_WIDTH_ADDR )
    {
        const long centerX = ( long ) viewport.Width / 2;
        const long centerY = ( long ) viewport.Height / 2;

        RECT rect;
        rect.left    = centerX - 200;
        rect.right   = centerX + 200;
        rect.top     = 0;
        rect.bottom  = 20;

        font->DrawText (
            0,                              // Text as a ID3DXSprite object
            "Lorem ipsum dolor sit amet",   // Text as a C-string
            -1,                             // Number of letters, -1 for null-terminated
            &rect,                          // Text bounding RECT
            DT_CENTER,                      // Text formatting
            D3DCOLOR_XRGB ( 0, 255, 0 ) );  // Text color
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

extern "C" void callback();

static const Asm hookCallback1 =
{
    MM_HOOK_CALL1_ADDR,
    {
        0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - MM_HOOK_CALL1_ADDR - 5 ),   // call callback
        0xE9, INLINE_DWORD ( MM_HOOK_CALL2_ADDR - MM_HOOK_CALL1_ADDR - 10 )         // jmp MM_HOOK_CALL2_ADDR
    }
};

void initializePreHacks()
{
    WRITE_ASM_HACK ( hookCallback1 );
    WRITE_ASM_HACK ( hookCallback2 );
    WRITE_ASM_HACK ( loopStartJump ); // Write the jump location last, due to dependencies on the above

    for ( const Asm& hack : enableDisabledStages )
        WRITE_ASM_HACK ( hack );

    WRITE_ASM_HACK ( fixRyougiStageMusic1 );
    WRITE_ASM_HACK ( fixRyougiStageMusic2 );

    for ( const Asm& hack : hijackControls )
        WRITE_ASM_HACK ( hack );
}

void initializePostHacks()
{
    if ( detectWine() )
        return;

    WRITE_ASM_HACK ( disableFpsLimit );

    // Hook DirectX
    void *hwnd;
    string err;
    if ( ( hwnd = enumFindWindow ( CC_TITLE ) ) == 0 )
        LOG ( "Couldn't find window '%s'", CC_TITLE );
    else if ( ! ( err = InitDirectX ( hwnd ) ).empty() )
        LOG ( "InitDirectX failed: %s", err );
    else if ( ! ( err = HookDirectX() ).empty() )
        LOG ( "HookDirectX failed: %s", err );
}

void deinitializeHacks()
{
    UnhookDirectX();

    loopStartJump.revert();
    hookCallback2.revert();
    hookCallback1.revert();
}
