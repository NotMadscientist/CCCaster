#include "DllOverlayUi.h"
#include "DllHacks.h"
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

#define OVERLAY_DEBUG_COLOR             D3DCOLOR_XRGB ( 255, 0, 0 )

#define OVERLAY_TEXT_BORDER             ( 10 )

#define OVERLAY_SELECTOR_L_COLOR        D3DCOLOR_XRGB ( 210, 0, 0 )

#define OVERLAY_SELECTOR_R_COLOR        D3DCOLOR_XRGB ( 30, 30, 255 )

#define OVERLAY_SELECTOR_X_BORDER       ( 5 )

#define OVERLAY_SELECTOR_Y_BORDER       ( 1 )

#define OVERLAY_BG_COLOR                D3DCOLOR_ARGB ( 220, 0, 0, 0 )

#define OVERLAY_CHANGE_DELTA            ( 4 + abs ( height - newHeight ) / 4 )

#define INLINE_RECT(rect)               rect.left, rect.top, rect.right, rect.bottom


ENUM ( State, Disabled, Disabling, Enabled, Enabling );

static State state = State::Disabled;

static int height = 0, oldHeight = 0, newHeight = 0;

static int initialTimeout = 0, messageTimeout = 0;

static array<string, 3> text;

static array<RECT, 2> selector;

static array<bool, 2> shouldDrawSelector { false, false };

static ID3DXFont *font = 0;

static IDirect3DVertexBuffer9 *background = 0;


namespace DllOverlayUi
{

void enable()
{
    if ( ProcessManager::isWine() )
        return;

    if ( state != State::Enabled )
        state = State::Enabling;
}

void disable()
{
    if ( ProcessManager::isWine() )
        return;

    if ( state != State::Disabled )
        state = State::Disabling;
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

static inline int getTextHeight ( const array<string, 3>& newText )
{
    int height = 0;

    for ( const string& text : newText )
        height = max ( height, OVERLAY_FONT_HEIGHT * ( 1 + count ( text.begin(), text.end(), '\n' ) ) );

    return height;
}

void updateText ( const array<string, 3>& newText )
{
    if ( ProcessManager::isWine() )
        return;

    switch ( state.value )
    {
        default:
        case State::Disabled:
            height = oldHeight = newHeight = 0;
            text = { "", "", "" };
            return;

        case State::Disabling:
            newHeight = 0;

            if ( height != newHeight )
                break;

            state = State::Disabled;
            oldHeight = 0;
            text = { "", "", "" };
            break;

        case State::Enabled:
            newHeight = getTextHeight ( newText );

            if ( newHeight > height )
                break;

            if ( newHeight == height )
                oldHeight = height;

            text = newText;
            break;

        case State::Enabling:
            newHeight = getTextHeight ( newText );

            if ( height != newHeight )
                break;

            state = State::Enabled;
            oldHeight = height;
            break;
    }

    if ( height == newHeight )
        return;

    if ( newHeight > height )
        height = clamped ( height + OVERLAY_CHANGE_DELTA, height, newHeight );
    else
        height = clamped ( height - OVERLAY_CHANGE_DELTA, newHeight, height );
}

bool isEnabled()
{
    if ( ProcessManager::isWine() )
        return false;

    return ( state != State::Disabled ) && ( messageTimeout <= 0 );
}


void showMessage ( const string& newText, int timeout )
{
    if ( ProcessManager::isWine() )
        return;

    // Get timeout in frames
    initialTimeout = messageTimeout = ( timeout / 17 );

    // Show the message in the middle
    text = { "", "", newText };
    shouldDrawSelector = { false, false };

    enable();
}

void updateMessage()
{
    if ( ProcessManager::isWine() )
        return;

    updateText ( text );

    if ( messageTimeout == 1 )
    {
        if ( state == State::Disabled )
            messageTimeout = 0;
        return;
    }

    if ( messageTimeout <= 2 )
    {
        disable();
        messageTimeout = 1;
        return;
    }

    // Reset message timeout when backgrounded
    if ( DllHacks::windowHandle != GetForegroundWindow() )
        messageTimeout = initialTimeout;
    else
        --messageTimeout;
}

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

void updateSelector ( uint8_t index, int position, const string& line )
{
    if ( index > 1 )
        return;

    if ( position == 0 || line.empty() )
    {
        shouldDrawSelector[index] = false;
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

    selector[index] = rect;
    shouldDrawSelector[index] = true;
}

bool isShowingMessage()
{
    if ( ProcessManager::isWine() )
        return false;

    return ( messageTimeout > 0 );
}

#ifndef RELEASE

string debugText;

int debugTextAlign = 0;

#endif // NOT RELEASE

} // namespace DllOverlayUi


struct Vertex
{
    FLOAT x, y, z;
    DWORD color;

    static const DWORD Format = ( D3DFVF_XYZ | D3DFVF_DIFFUSE );
};


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

void initOverlayText ( IDirect3DDevice9 *device )
{
    D3DXCreateFont ( device,                                    // device pointer
                     OVERLAY_FONT_HEIGHT,                       // height
                     OVERLAY_FONT_WIDTH,                        // width
                     OVERLAY_FONT_WEIGHT,                       // weight
                     1,                                         // # of mipmap levels
                     false,                                     // italic
                     DEFAULT_CHARSET,                           // charset
                     OUT_DEFAULT_PRECIS,                        // output precision
                     ANTIALIASED_QUALITY,                       // quality
                     DEFAULT_PITCH | FF_DONTCARE,               // pitch and family
                     OVERLAY_FONT,                              // typeface name
                     &font );                                   // pointer to ID3DXFont

    static const Vertex verts[4] =
    {
        { -1, -1, 0, OVERLAY_BG_COLOR },
        {  1, -1, 0, OVERLAY_BG_COLOR },
        { -1,  1, 0, OVERLAY_BG_COLOR },
        {  1,  1, 0, OVERLAY_BG_COLOR },
    };

    device->CreateVertexBuffer ( 4 * sizeof ( Vertex ),         // buffer size in bytes
                                 0,                             // memory usage flags
                                 Vertex::Format,                // vertex format
                                 D3DPOOL_MANAGED,               // memory storage flags
                                 &background,                    // pointer to IDirect3DVertexBuffer9
                                 0 );                           // unused

    void *ptr = 0;

    background->Lock ( 0, 0, ( void ** ) &ptr, 0 );
    memcpy ( ptr, verts, 4 * sizeof ( verts[0] ) );
    background->Unlock();
}

void invalidateOverlayText()
{
    font->OnLostDevice();
    font = 0;

    background->Release();
    background = 0;
}

void renderOverlayText ( IDirect3DDevice9 *device, const D3DVIEWPORT9& viewport )
{
#ifndef RELEASE

    if ( !debugText.empty() )
    {
        RECT rect;
        rect.top = rect.left = 0;
        rect.right = viewport.Width;
        rect.bottom = viewport.Height;

        DrawText ( debugText, rect, DT_WORDBREAK |
                   ( debugTextAlign == 0 ? DT_CENTER : ( debugTextAlign < 0 ? DT_LEFT : DT_RIGHT ) ),
                   OVERLAY_DEBUG_COLOR );
    }

#endif // RELEASE

    if ( state == State::Disabled )
        return;


    // Calculate message width if showing one
    float messageWidth = 0.0f;
    if ( isShowingMessage() )
    {
        RECT rect;
        rect.top = rect.left = 0;
        rect.right = 1;
        rect.bottom = OVERLAY_FONT_HEIGHT;

        DrawText ( text[2], rect, DT_CALCRECT, D3DCOLOR_XRGB ( 0, 0, 0 ) );

        messageWidth = rect.right + 2 * OVERLAY_TEXT_BORDER;
    }

    // Scaling factor for the overlay background
    const float scaleX = ( isShowingMessage() ? messageWidth / viewport.Width : 1.0f );
    const float scaleY = float ( height + 2 * OVERLAY_TEXT_BORDER ) / viewport.Height;

    D3DXMATRIX translate, scale;
    D3DXMatrixScaling ( &scale, scaleX, scaleY, 1.0f );
    D3DXMatrixTranslation ( &translate, 0.0f, 1.0f - scaleY, 0.0f );

    device->SetTexture ( 0, 0 );
    device->SetTransform ( D3DTS_VIEW, & ( scale = scale * translate ) );
    device->SetStreamSource ( 0, background, 0, sizeof ( Vertex ) );
    device->SetFVF ( Vertex::Format );
    device->DrawPrimitive ( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Only draw text if fully enabled or showing a message
    if ( state != State::Enabled )
        return;

    if ( ! ( text[0].empty() && text[1].empty() && text[2].empty() ) )
    {
        const int centerX = viewport.Width / 2;

        RECT rect;
        rect.left   = centerX - int ( ( viewport.Width / 2 ) * 1.0 ) + OVERLAY_TEXT_BORDER;
        rect.right  = centerX + int ( ( viewport.Width / 2 ) * 1.0 ) - OVERLAY_TEXT_BORDER;
        rect.top    = OVERLAY_TEXT_BORDER;
        rect.bottom = rect.top + height + OVERLAY_TEXT_BORDER;

        if ( newHeight == height )
        {
            if ( shouldDrawSelector[0] )
                DrawRectangle ( device, INLINE_RECT ( selector[0] ), OVERLAY_SELECTOR_L_COLOR );

            if ( shouldDrawSelector[1] )
                DrawRectangle ( device, INLINE_RECT ( selector[1] ), OVERLAY_SELECTOR_R_COLOR );
        }

        if ( !text[2].empty() )
            DrawText ( text[2], rect, DT_WORDBREAK | DT_CENTER, OVERLAY_TEXT_COLOR );

        if ( !text[1].empty() )
            DrawText ( text[1], rect, DT_WORDBREAK | DT_RIGHT, OVERLAY_TEXT_COLOR );

        if ( !text[0].empty() )
            DrawText ( text[0], rect, DT_WORDBREAK | DT_LEFT, OVERLAY_TEXT_COLOR );
    }
}
