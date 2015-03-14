#include "DllOverlayUi.h"
#include "DllOverlayPrimitives.h"
#include "ProcessManager.h"

#define M_PI 3.14159265358979323846
#include <cmath>

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;


static bool showing = false;

static IDirect3DVertexBuffer9 *background = 0;

static IDirect3DTexture9 *light = 0, *dark = 0;

static int selectorX = 0, selectorY = 0;

static D3DCOLOR color = D3DCOLOR_XRGB ( 255, 255, 255 );


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

void updateColorSelector ( int x, int y )
{
    selectorX = x;
    selectorY = y;
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

    void *ptr = 0;

    background->Lock ( 0, 0, ( void ** ) &ptr, 0 );
    memcpy ( ptr, verts, 4 * sizeof ( verts[0] ) );
    background->Unlock();

    HRESULT result =
        D3DXCreateTextureFromFile ( device, ( ProcessManager::appDir + FOLDER "wheel_light.png" ).c_str(), &light );

    if ( FAILED ( result ) )
    {
        LOG ( "D3DXCreateTextureFromFile failed: 0x%08x", result );
    }

    result = D3DXCreateTextureFromFile ( device, ( ProcessManager::appDir + FOLDER "wheel_dark.png" ).c_str(), &dark );

    if ( FAILED ( result ) )
    {
        LOG ( "D3DXCreateTextureFromFile failed: 0x%08x", result );
    }
}

void invalidatePaletteSelector()
{
    background->Release();
    background = 0;

    light->Release();
    light = 0;

    dark->Release();
    dark = 0;
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

    device->SetRenderState ( D3DRS_LIGHTING, FALSE );
    device->SetRenderState ( D3DRS_ALPHABLENDENABLE, TRUE );
    device->SetRenderState ( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    device->SetRenderState ( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    const float scaleX = 128.0f / viewport.Width;
    const float scaleY = 128.0f / viewport.Height;

    D3DXMATRIX scale;
    D3DXMatrixScaling ( &scale, scaleX, scaleY, 1.0f );

    device->SetTexture ( 0, light );
    device->SetTransform ( D3DTS_VIEW, &scale );
    device->SetStreamSource ( 0, background, 0, sizeof ( TextureVertex ) );
    device->SetFVF ( TextureVertex::Format );
    device->DrawPrimitive ( D3DPT_TRIANGLEFAN, 0, 2 );

    const int centerX = viewport.Width / 2;
    const int centerY = viewport.Height / 2;

    const int deltaX = selectorX - centerX;
    const int deltaY = selectorY - centerY;

    const int delta2 = clamped ( deltaX * deltaX + deltaY * deltaY, 0, 64 * 64 );

    if ( delta2 <= 64 * 64 )
    {
        double angle = atan2 ( deltaY, -deltaX );
        double delta = sqrt ( ( double ) delta2 );

        uint8_t h = ( 255 * ( angle + M_PI ) ) / ( 2 * M_PI );
        uint8_t s = ( 255 * delta ) / 64;
        uint8_t v = 255;

        color = hsv2rgb ( h, s, v );
    }

    DrawRectangle ( device, centerX - 30, viewport.Height - 60, centerX + 30, viewport.Height, color );

    // D3DXMATRIX translate, scale, final;
    // D3DXMatrixScaling ( &scale, scaleX, scaleY, 1.0f );
    // D3DXMatrixTranslation ( &translate, -0.5f, 0.0f, 0.0f );

    // device->SetTexture ( 0, light );
    // device->SetTransform ( D3DTS_VIEW, & ( final = scale * translate ) );
    // device->SetStreamSource ( 0, background, 0, sizeof ( TextureVertex ) );
    // device->SetFVF ( TextureVertex::Format );
    // device->DrawPrimitive ( D3DPT_TRIANGLEFAN, 0, 2 );

    // D3DXMatrixTranslation ( &translate, 0.5f, 0.0f, 0.0f );

    // device->SetTexture ( 0, dark );
    // device->SetTransform ( D3DTS_VIEW, & ( final = scale * translate ) );
    // device->DrawPrimitive ( D3DPT_TRIANGLEFAN, 0, 2 );

    // const int centerX = viewport.Width / 2;
    // const int centerY = viewport.Height / 2;

    // if ( selectorX < centerX )
    // {
    //     const int deltaX = selectorX - centerX;
    //     const int deltaY = selectorY - centerY;

    //     const int delta2 = clamped ( deltaX * deltaX + deltaY * deltaY, 0, 64 * 64 );

    //     if ( delta2 <= 64 * 64 )
    //     {
    //         double angle = atan2 ( deltaY, -deltaX );
    //         double delta = sqrt ( ( double ) delta2 );

    //         uint8_t h = ( 255 * ( angle + M_PI ) ) / ( 2 * M_PI );
    //         uint8_t s = ( 255 * delta ) / 64;
    //         uint8_t v = 255;
    //         // uint8_t s = 255;
    //         // uint8_t v = ( 255 * delta ) / 64;

    //         color = hsv2rgb ( h, s, v );
    //     }
    // }

    // DrawRectangle ( device, centerX - 30, viewport.Height - 60, centerX + 30, viewport.Height, color );
}
