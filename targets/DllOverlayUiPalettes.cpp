#include "DllOverlayUi.h"
#include "ProcessManager.h"

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;


static bool showing = false;

static IDirect3DVertexBuffer9 *background = 0;

static IDirect3DTexture9 *texture = 0;


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
        D3DXCreateTextureFromFile ( device, ( ProcessManager::appDir + FOLDER "wheel.png" ).c_str(), &texture );

    if ( FAILED ( result ) )
    {
        LOG ( "D3DXCreateTextureFromFile failed: 0x%08x", result );
    }
}

void invalidatePaletteSelector()
{
    background->Release();
    background = 0;

    texture->Release();
    texture = 0;
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

    device->SetTexture ( 0, texture );
    device->SetTransform ( D3DTS_VIEW, &scale );
    device->SetStreamSource ( 0, background, 0, sizeof ( TextureVertex ) );
    device->SetFVF ( TextureVertex::Format );
    device->DrawPrimitive ( D3DPT_TRIANGLEFAN, 0, 2 );
}
