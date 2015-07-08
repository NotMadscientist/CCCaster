#pragma once

#define M_PI 3.14159265358979323846
#include <cmath>
#include <string>
#include <array>

#include <windows.h>
#include <d3dx9.h>


#define COLOR_BLACK D3DCOLOR_XRGB ( 0, 0, 0 )
#define COLOR_WHITE D3DCOLOR_XRGB ( 255, 255, 255 )
#define COLOR_RED   D3DCOLOR_XRGB ( 255, 0, 0 )
#define COLOR_GREEN D3DCOLOR_XRGB ( 0, 255, 0 )
#define COLOR_BLUE  D3DCOLOR_XRGB ( 0, 0, 255 )


static inline void DrawRectangle ( IDirect3DDevice9 *device, int x1, int y1, int x2, int y2, const D3DCOLOR& color )
{
    const D3DRECT rect = { x1, y1, x2, y2 };
    device->Clear ( 1, &rect, D3DCLEAR_TARGET, color, 0, 0 );
}

static inline void DrawBox ( IDirect3DDevice9 *device, int x1, int y1, int x2, int y2, int w, const D3DCOLOR& color )
{
    DrawRectangle ( device, x1, y1, x1 + w, y2, color );
    DrawRectangle ( device, x1, y1, x2, y1 + w, color );
    DrawRectangle ( device, x2 - w, y1, x2, y2, color );
    DrawRectangle ( device, x1, y2 - w, x2, y2, color );
}

template<size_t N>
static inline void DrawCircle ( IDirect3DDevice9 *device, float x, float y, float r, const D3DCOLOR& color )
{
    std::array < D3DXVECTOR2, N + 1 > verts;

    for ( size_t i = 0; i < verts.size(); ++i )
    {
        verts[i].x = x + r * cos ( float ( 2 * M_PI * i ) / ( verts.size() - 1 ) );
        verts[i].y = y + r * sin ( float ( 2 * M_PI * i ) / ( verts.size() - 1 ) );
    }

    ID3DXLine *line;
    D3DXCreateLine ( device, &line );

    line->Begin();
    line->Draw ( &verts[0], verts.size(), color );
    line->End();
    line->Release();
}

static inline int DrawText ( ID3DXFont *font, const std::string& text, RECT& rect, int flags, const D3DCOLOR& color )
{
    if ( ! font )
        return 0;

    return font->DrawText ( 0,                                  // text as a ID3DXSprite object
                            &text[0],                           // text buffer
                            text.size(),                        // number of characters, -1 if null-terminated
                            &rect,                              // text bounding RECT
                            flags | DT_NOCLIP,                  // text formatting
                            color );                            // text color
}
