#include "DllOverlayUi.h"
#include "Constants.h"

#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;


static bool initalizedDirectX = false;

static bool shouldInitDirectX = false;


namespace DllOverlayUi
{

void init()
{
    shouldInitDirectX = true;
}

} // namespace DllOverlayUi


void initOverlayText ( IDirect3DDevice9 *device );

void invalidateOverlayText();

void renderOverlayText ( IDirect3DDevice9 *device, const D3DVIEWPORT9& viewport );


void initPaletteSelector ( IDirect3DDevice9 *device );

void invalidatePaletteSelector();

void renderPaletteSelector ( IDirect3DDevice9 *device, const D3DVIEWPORT9& viewport );


void InitializeDirectX ( IDirect3DDevice9 *device )
{
    if ( !shouldInitDirectX )
        return;

    initalizedDirectX = true;

    initOverlayText ( device );

    initPaletteSelector ( device );
}

void InvalidateDeviceObjects()
{
    if ( !initalizedDirectX )
        return;

    initalizedDirectX = false;

    invalidateOverlayText();

    invalidatePaletteSelector();
}


// Note: this is called on the SAME thread as the main application thread
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    if ( !initalizedDirectX )
        InitializeDirectX ( device );

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // Only draw in the main viewport; there should only be one with this width
    if ( viewport.Width != * CC_SCREEN_WIDTH_ADDR )
        return;

    renderOverlayText ( device, viewport );

    renderPaletteSelector ( device, viewport );
}
