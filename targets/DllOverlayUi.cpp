#include "DllOverlayUi.h"
#include "ProcessManager.h"
#include "Enum.h"

#include <windows.h>
#include <d3dx9.h>

using namespace std;
using namespace DllOverlayUi;


#define OVERLAY_FONT                    "Tahoma"

#define OVERLAY_FONT_HEIGHT             ( 14 )

#define OVERLAY_FONT_WIDTH              ( 5 )

#define OVERLAY_FONT_WEIGHT             ( 600 )

#define OVERLAY_TEXT_COLOR              D3DCOLOR_XRGB ( 255, 255, 255 )

#define OVERLAY_TEXT_BORDER             ( 10 )

#define OVERLAY_SELECTOR_L_COLOR        D3DCOLOR_XRGB ( 210, 0, 0 )

#define OVERLAY_SELECTOR_R_COLOR        D3DCOLOR_XRGB ( 30, 30, 255 )

#define OVERLAY_SELECTOR_X_BORDER       ( 5 )

#define OVERLAY_SELECTOR_Y_BORDER       ( 1 )

#define OVERLAY_BG_COLOR                D3DCOLOR_ARGB ( 220, 0, 0, 0 )

#define OVERLAY_CHANGE_DELTA            ( 4 + abs ( overlayHeight - newHeight ) / 4 )

#define INLINE_RECT(rect)               rect.left, rect.top, rect.right, rect.bottom



namespace DllOverlayUi
{

ENUM ( Overlay, Disabled, Disabling, Enabled, Enabling );

static Overlay overlayState = Overlay::Disabled;

static int overlayHeight = 0, oldHeight = 0, newHeight = 0, messageTimeout = 0;

static array<string, 3> overlayText;

static array<RECT, 2> overlaySelector;

static array<bool, 2> drawSelector { false, false };


static inline int getTextHeight ( const array<string, 3>& newText )
{
    int height = 0;

    for ( const string& text : newText )
        height = max ( height, OVERLAY_FONT_HEIGHT * ( 1 + count ( text.begin(), text.end(), '\n' ) ) );

    return height;
}


void enable()
{
    if ( ProcessManager::isWine() )
        return;

    if ( overlayState != Overlay::Enabled )
        overlayState = Overlay::Enabling;
}

void disable()
{
    if ( ProcessManager::isWine() )
        return;

    if ( overlayState != Overlay::Disabled )
        overlayState = Overlay::Disabling;
}

void toggle()
{
    if ( ProcessManager::isWine() )
        return;

    if ( isEnabled() )
        disable();
    else
        enable();
}

void updateText ( const array<string, 3>& text )
{
    if ( ProcessManager::isWine() )
        return;

    switch ( overlayState.value )
    {
        default:
        case Overlay::Disabled:
            overlayHeight = oldHeight = newHeight = 0;
            overlayText = { "", "", "" };
            return;

        case Overlay::Disabling:
            newHeight = 0;

            if ( overlayHeight != newHeight )
                break;

            overlayState = Overlay::Disabled;
            oldHeight = 0;
            overlayText = { "", "", "" };
            break;

        case Overlay::Enabled:
            newHeight = getTextHeight ( text );

            if ( newHeight > overlayHeight )
                break;

            if ( newHeight == overlayHeight )
                oldHeight = overlayHeight;

            overlayText = text;
            break;

        case Overlay::Enabling:
            newHeight = getTextHeight ( text );

            if ( overlayHeight != newHeight )
                break;

            overlayState = Overlay::Enabled;
            oldHeight = overlayHeight;
            break;
    }

    if ( overlayHeight == newHeight )
        return;

    if ( newHeight > overlayHeight )
        overlayHeight = clamped ( overlayHeight + OVERLAY_CHANGE_DELTA, overlayHeight, newHeight );
    else
        overlayHeight = clamped ( overlayHeight - OVERLAY_CHANGE_DELTA, newHeight, overlayHeight );
}

bool isEnabled()
{
    if ( ProcessManager::isWine() )
        return false;

    return ( overlayState != Overlay::Disabled ) && ( messageTimeout <= 0 );
}


void showMessage ( const string& text, int timeout )
{
    if ( ProcessManager::isWine() )
        return;

    messageTimeout = ( timeout / 17 );
    overlayText = { "", "", text };
    drawSelector = { false, false };

    enable();
}

void updateMessage()
{
    if ( ProcessManager::isWine() )
        return;

    updateText ( overlayText );

    if ( messageTimeout == 1 )
    {
        if ( overlayState == Overlay::Disabled )
            messageTimeout = 0;
        return;
    }

    if ( messageTimeout <= 2 )
    {
        disable();
        messageTimeout = 1;
        return;
    }

    --messageTimeout;
}

bool isShowingMessage()
{
    if ( ProcessManager::isWine() )
        return false;

    return ( messageTimeout > 0 );
}


static bool initalizedDirectX = false;

static ID3DXFont *font = 0;

static IDirect3DVertexBuffer9 *background = 0;


struct Vertex
{
    FLOAT x, y, z;
    DWORD color;

    static const DWORD format = ( D3DFVF_XYZ | D3DFVF_DIFFUSE );
};


static inline int DrawText ( const string& text, RECT& rect, int flags, const D3DCOLOR& color )
{
    if ( !font )
        return 0;

    return font->DrawText ( 0,                                  // text as a ID3DXSprite object
                            &text[0],                           // text buffer
                            text.size(),                        // number of characters, -1 if null-terminated
                            &rect,                              // text bounding RECT
                            flags | DT_NOCLIP,                  // text formatting
                            color );                            // text colour
}

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

void updateSelector ( uint8_t index, int position, const string& line )
{
    if ( index > 1 )
        return;

    if ( position == 0 || line.empty() )
    {
        drawSelector[index] = false;
        return;
    }

    RECT rect;
    rect.top = rect.left = 0;
    rect.right = 1;
    rect.bottom = OVERLAY_FONT_HEIGHT;
    DrawText ( line, rect, DT_CALCRECT, D3DCOLOR_XRGB ( 0, 0, 0 ) );

    rect.top    += OVERLAY_TEXT_BORDER + position * OVERLAY_FONT_HEIGHT - OVERLAY_SELECTOR_Y_BORDER + 1;
    rect.bottom += OVERLAY_TEXT_BORDER + position * OVERLAY_FONT_HEIGHT + OVERLAY_SELECTOR_Y_BORDER;

    if ( index == 0 )
    {
        rect.left  += OVERLAY_TEXT_BORDER - OVERLAY_SELECTOR_X_BORDER;
        rect.right += OVERLAY_TEXT_BORDER + OVERLAY_SELECTOR_X_BORDER;
    }
    else
    {
        rect.left  = ( * CC_SCREEN_WIDTH_ADDR ) - rect.right - OVERLAY_TEXT_BORDER - OVERLAY_SELECTOR_X_BORDER;
        rect.right = ( * CC_SCREEN_WIDTH_ADDR ) - OVERLAY_TEXT_BORDER + OVERLAY_SELECTOR_X_BORDER;
    }

    overlaySelector[index] = rect;
    drawSelector[index] = true;
}

} // namespace DllOverlayUi


// Note: this is called on the SAME thread as the main application thread
void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    if ( !initalizedDirectX )
    {
        initalizedDirectX = true;

        D3DXCreateFont ( device,                                // device pointer
                         OVERLAY_FONT_HEIGHT,                   // height
                         OVERLAY_FONT_WIDTH,                    // width
                         OVERLAY_FONT_WEIGHT,                   // weight
                         1,                                     // # of mipmap levels
                         false,                                 // italic
                         DEFAULT_CHARSET,                       // charset
                         OUT_DEFAULT_PRECIS,                    // output precision
                         ANTIALIASED_QUALITY,                   // quality
                         DEFAULT_PITCH | FF_DONTCARE,           // pitch and family
                         OVERLAY_FONT,                          // typeface name
                         &font );                               // pointer to ID3DXFont

        static const Vertex verts[4] =
        {
            { -1.0, -1.0, 0.0f, OVERLAY_BG_COLOR },
            {  1.0, -1.0, 0.0f, OVERLAY_BG_COLOR },
            { -1.0,  1.0, 0.0f, OVERLAY_BG_COLOR },
            {  1.0,  1.0, 0.0f, OVERLAY_BG_COLOR }
        };

        device->CreateVertexBuffer ( 4 * sizeof ( Vertex ),     // buffer size in bytes
                                     0,                         // memory usage flags
                                     Vertex::format,            // vertex format
                                     D3DPOOL_MANAGED,           // memory storage flags
                                     &background,               // pointer to IDirect3DVertexBuffer9
                                     0 );                       // unused

        void *ptr = 0;

        background->Lock ( 0, 0, ( void ** ) &ptr, 0 );
        memcpy ( ptr, verts, 4 * sizeof ( verts[0] ) );
        background->Unlock();
    }

    if ( overlayState == Overlay::Disabled )
        return;

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // Only draw in the main viewport; there should only be one with this width
    if ( viewport.Width != * CC_SCREEN_WIDTH_ADDR )
        return;

    // Calculate message width if showing one
    float messageWidth = 0.0f;
    if ( isShowingMessage() )
    {
        RECT rect;
        rect.top = rect.left = 0;
        rect.right = 1;
        rect.bottom = OVERLAY_FONT_HEIGHT;

        DrawText ( overlayText[2], rect, DT_CALCRECT, D3DCOLOR_XRGB ( 0, 0, 0 ) );

        messageWidth = rect.right + 2 * OVERLAY_TEXT_BORDER;
    }

    // Scaling factor for the overlay background
    const float scaleX = ( isShowingMessage() ? messageWidth / viewport.Width : 1.0f );
    const float scaleY = float ( overlayHeight + 2 * OVERLAY_TEXT_BORDER ) / viewport.Height;

    D3DXMATRIX translate, scale;
    D3DXMatrixScaling ( &scale, scaleX, scaleY, 1.0f );
    D3DXMatrixTranslation ( &translate, 0.0f, 1.0f - scaleY, 0.0f );
    device->SetTransform ( D3DTS_VIEW, & ( scale = scale * translate ) );

    device->SetStreamSource ( 0, background, 0, sizeof ( Vertex ) );
    device->SetFVF ( Vertex::format );
    device->DrawPrimitive ( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Only draw text if fully enabled or showing a message
    if ( overlayState != Overlay::Enabled )
        return;

    if ( ! ( overlayText[0].empty() && overlayText[1].empty() && overlayText[2].empty() ) )
    {
        const int centerX = viewport.Width / 2;

        RECT rect;
        rect.left   = centerX - int ( ( viewport.Width / 2 ) * 1.0 ) + OVERLAY_TEXT_BORDER;
        rect.right  = centerX + int ( ( viewport.Width / 2 ) * 1.0 ) - OVERLAY_TEXT_BORDER;
        rect.top    = OVERLAY_TEXT_BORDER;
        rect.bottom = rect.top + overlayHeight + OVERLAY_TEXT_BORDER;

        if ( newHeight == overlayHeight )
        {
            if ( drawSelector[0] )
                DrawRectangle ( device, INLINE_RECT ( overlaySelector[0] ), OVERLAY_SELECTOR_L_COLOR );

            if ( drawSelector[1] )
                DrawRectangle ( device, INLINE_RECT ( overlaySelector[1] ), OVERLAY_SELECTOR_R_COLOR );
        }

        if ( !overlayText[2].empty() )
            DrawText ( overlayText[2], rect, DT_WORDBREAK | DT_CENTER, OVERLAY_TEXT_COLOR );

        if ( !overlayText[1].empty() )
            DrawText ( overlayText[1], rect, DT_WORDBREAK | DT_RIGHT, OVERLAY_TEXT_COLOR );

        if ( !overlayText[0].empty() )
            DrawText ( overlayText[0], rect, DT_WORDBREAK | DT_LEFT, OVERLAY_TEXT_COLOR );
    }
}

void PresentFrameEnd ( IDirect3DDevice9 *device )
{
}

void InvalidateDeviceObjects()
{
    if ( initalizedDirectX )
    {
        initalizedDirectX = false;

        font->OnLostDevice();
        font = 0;

        background->Release();
        background = 0;
    }
}
