#include "DllOverlayUi.h"
#include "DllOverlayPrimitives.h"
#include "ProcessManager.h"
#include "TimerManager.h"

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;



static const D3DXVECTOR2 paletteCoord = { 0.5, 0.5 };

static const float paletteRadius = 64;


static bool showing = false;

static IDirect3DVertexBuffer9 *background = 0;

static IDirect3DTexture9 *lightWheel = 0, *darkWheel = 0;

static D3DCOLOR color = COLOR_WHITE;

static float angle = 0, radius = 0;

static bool useLight = true;

static bool doubleClick = false;

static D3DXVECTOR2 selectorPos = { 0, 0 };


namespace DllOverlayUi
{

void showPalettes()
{
    if ( ProcessManager::isWine() )
        return;

    if ( isEnabled() )
        return;

    showing = true;
}

void hidePalettes()
{
    if ( ProcessManager::isWine() )
        return;

    showing = false;
}

void togglePalettes()
{
    if ( ProcessManager::isWine() )
        return;

    if ( isShowingPalettes() )
        hidePalettes();
    else
        showPalettes();
}

bool isShowingPalettes()
{
    if ( ProcessManager::isWine() )
        return false;

    return showing;
}

void paletteMouseEvent ( int x, int y, bool isDown, bool pressed, bool released )
{
    if ( isDown )
    {
        selectorPos.x = x;
        selectorPos.y = y;
    }

    if ( pressed )
    {
        static uint64_t lastClickTime = 0;

        const uint64_t now = TimerManager::get().getNow ( true );

        if ( now - lastClickTime <= GetDoubleClickTime() )
            doubleClick = true;

        lastClickTime = now;
    }
}

} // namespace DllOverlayUi


struct TextureVertex
{
    FLOAT x, y, z;
    FLOAT tu, tv;

    static const DWORD Format = ( D3DFVF_XYZ | D3DFVF_TEX1 );
};


void initPaletteSelector ( IDirect3DDevice9 *device )
{
    static const TextureVertex verts[4] =
    {
        { -1, -1, 0, 0, 0 },
        {  1, -1, 0, 1, 0 },
        {  1,  1, 0, 1, 1 },
        { -1,  1, 0, 0, 1 },
    };

    device->CreateVertexBuffer ( 4 * sizeof ( TextureVertex ),  // buffer size in bytes
                                 0,                             // memory usage flags
                                 TextureVertex::Format,         // vertex format
                                 D3DPOOL_MANAGED,               // memory storage flags
                                 &background,                      // pointer to IDirect3DVertexBuffer9
                                 0 );                           // unused

    void *ptr;
    HRESULT result;

    background->Lock ( 0, 0, ( void ** ) &ptr, 0 );
    memcpy ( ptr, verts, 4 * sizeof ( verts[0] ) );
    background->Unlock();

    result = D3DXCreateTextureFromFile (
                 device, ( ProcessManager::appDir + FOLDER "wheel_light.png" ).c_str(), &lightWheel );

    if ( FAILED ( result ) )
    {
        LOG ( "D3DXCreateTextureFromFile failed: 0x%08x", result );
    }

    result = D3DXCreateTextureFromFile (
                 device, ( ProcessManager::appDir + FOLDER "wheel_dark.png" ).c_str(), &darkWheel );

    if ( FAILED ( result ) )
    {
        LOG ( "D3DXCreateTextureFromFile failed: 0x%08x", result );
    }
}

void invalidatePaletteSelector()
{
    background->Release();
    background = 0;

    lightWheel->Release();
    lightWheel = 0;

    darkWheel->Release();
    darkWheel = 0;
}

static inline D3DCOLOR hsv2rgb ( uint8_t h, uint8_t s, uint8_t v )
{
    uint8_t region, remainder, p, q, t;

    if ( s == 0 )
        return D3DCOLOR_XRGB ( v, v, v );

    region = h / 43;
    remainder = ( h - ( region * 43 ) ) * 6;

    p = ( v * ( 255 - s ) ) >> 8;
    q = ( v * ( 255 - ( ( s * remainder ) >> 8 ) ) ) >> 8;
    t = ( v * ( 255 - ( ( s * ( 255 - remainder ) ) >> 8 ) ) ) >> 8;

    switch ( region )
    {
        case 0:
            return D3DCOLOR_XRGB ( v, t, p );

        case 1:
            return D3DCOLOR_XRGB ( q, v, p );

        case 2:
            return D3DCOLOR_XRGB ( p, v, t );

        case 3:
            return D3DCOLOR_XRGB ( p, q, v );

        case 4:
            return D3DCOLOR_XRGB ( t, p, v );

        default:
            return D3DCOLOR_XRGB ( v, p, q );
    }
}

void renderPaletteSelector ( IDirect3DDevice9 *device, const D3DVIEWPORT9& viewport )
{
    if ( isEnabled() )
        showing = false;

    if ( !showing )
        return;

    const int centerX = viewport.Width / 2;
    const int centerY = viewport.Height / 2;

    device->SetRenderState ( D3DRS_LIGHTING, FALSE );
    device->SetRenderState ( D3DRS_ALPHABLENDENABLE, TRUE );
    device->SetRenderState ( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    device->SetRenderState ( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    const float scaleX = 2 * paletteRadius / viewport.Width;
    const float scaleY = 2 * paletteRadius / viewport.Height;

    D3DXMATRIX translate, scale;
    D3DXMatrixScaling ( &scale, scaleX, scaleY, 1.0f );
    D3DXMatrixTranslation ( &translate, paletteCoord.x, paletteCoord.y, 0.0f );

    device->SetTexture ( 0, ( useLight ? lightWheel : darkWheel ) );
    device->SetTransform ( D3DTS_VIEW, & ( scale = scale * translate ) );
    device->SetStreamSource ( 0, background, 0, sizeof ( TextureVertex ) );
    device->SetFVF ( TextureVertex::Format );
    device->DrawPrimitive ( D3DPT_TRIANGLEFAN, 0, 2 );

    const D3DXVECTOR2 paletteCenter = { centerX + paletteCoord.x * centerX, centerY - paletteCoord.y * centerY };
    const D3DXVECTOR2 delta = ( selectorPos - paletteCenter );
    const float delta2 = pow ( delta.x, 2.0f ) + pow ( delta.y, 2.0f );

    if ( delta2 <= pow ( 2 * paletteRadius, 2.0f ) )
    {
        angle = atan2 ( delta.y, delta.x );
        radius = sqrt ( ( float ) clamped ( delta2, 0.0f, pow ( paletteRadius, 2.0f ) ) );

        uint8_t h = ( 255 * ( angle + M_PI ) ) / ( 2 * M_PI );
        uint8_t s = ( useLight ? ( 255 * radius ) / paletteRadius : 255 );
        uint8_t v = ( useLight ? 255 : ( 255 * radius ) / paletteRadius );

        color = hsv2rgb ( h, s, v );
    }

    DrawCircle<10> ( device, paletteCenter.x + radius * cos ( angle ), paletteCenter.y + radius * sin ( angle ),
                     3, ( useLight ? COLOR_BLACK : COLOR_WHITE ) );

    DrawRectangle ( device, centerX - 30, viewport.Height - 60, centerX + 30, viewport.Height, color );

    if ( doubleClick )
    {
        doubleClick = false;

        if ( delta2 <= pow ( paletteRadius, 2.0f ) )
        {
            useLight = !useLight;
        }
    }
}
