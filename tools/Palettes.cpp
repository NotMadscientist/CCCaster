#include "PaletteManager.h"
#include "StringUtils.h"

#include "mbaacc_framedisplay.h"
#include "render.h"

#include <SDL.h>

#include <gl.h>
#include <glext.h>
#include <glu.h>

#include <windows.h>

#include <cstdarg>
#include <string>
#include <algorithm>
#include <unordered_map>

using namespace std;


#define EDITOR_FRAME_INTERVAL   ( 16 )

#define EDITOR_FONT             "Tahoma"

#define EDITOR_FONT_HEIGHT      ( 30 )

#define EDITOR_FONT_WIDTH       ( 0 )

#define EDITOR_FONT_WEIGHT      ( 600 )

#define EDITOR_FONT_COLOR       1.0, 1.0, 1.0

#define EDITOR_BAD_HEX_DELAY    ( 1000 )


#define SWAP_R_AND_B(COLOR) ( ( COLOR & 0xFF ) << 16 ) | ( COLOR & 0xFF00 ) | ( ( COLOR & 0xFF0000 ) >> 16 )


static MBAACC_FrameDisplay frameDisp;

static const RenderProperties renderProps = { 1, 0, 0, 0, 0, 0 };

static HWND hwnd = 0;
static HDC hdc = 0;
static HGLRC hrc = 0;

static GLuint font = 0;

static int screenWidth = 640;
static int screenHeight = 480;

static float spriteX = 0.3;
static float spriteY = 0.9;

static uint32_t ticker = 0;

static int color = 0, palette = 0;

static unordered_map<string, PaletteManager> palMans;

static enum { Navigation, CharacterEntry, PaletteEntry, ColorNumEntry, ColorHexEntry, } uiMode = Navigation;

static string colorHexStr;


string getClipboard()
{
    const char *buffer = "";

    if ( OpenClipboard ( 0 ) )
    {
        HANDLE hData = GetClipboardData ( CF_TEXT );
        buffer = ( const char * ) GlobalLock ( hData );
        if ( buffer == 0 )
            buffer = "";
        GlobalUnlock ( hData );
        CloseClipboard();
    }

    return string ( buffer );
}

void swapColorKeepAlpha ( uint32_t& a, uint32_t& b )
{
    const uint32_t tmp = a;
    a = ( tmp & 0xFF000000 ) | ( b & 0xFFFFFF );
    b = tmp & 0xFFFFFF;
}

void releaseFont()
{
    if ( !hwnd && !hdc && !hrc && !font )
        return;

    wglMakeCurrent ( 0, 0 );
    wglDeleteContext ( hrc );
    ReleaseDC ( hwnd, hdc );
    glDeleteLists ( font, 256 );

    hwnd = 0;
    hdc = 0;
    hrc = 0;
    font = 0;
}

void setupFont()
{
    releaseFont();

    hwnd = GetForegroundWindow();
    hdc = GetDC ( hwnd );
    hrc = wglCreateContext ( hdc );

    wglMakeCurrent ( hdc, hrc );

    HFONT winFont, oldFont;

    font = glGenLists ( 256 );                              // storage for 256 characters

    winFont = CreateFont ( EDITOR_FONT_HEIGHT,              // height
                           EDITOR_FONT_WIDTH,               // width
                           0,                               // angle of escapement
                           0,                               // orientation angle
                           EDITOR_FONT_WEIGHT,              // weight
                           FALSE,                           // italic
                           FALSE,                           // underline
                           FALSE,                           // strikeout
                           ANSI_CHARSET,                    // character set identifier
                           OUT_DEFAULT_PRECIS,              // output precision
                           CLIP_DEFAULT_PRECIS,             // clipping precision
                           ANTIALIASED_QUALITY,             // output quality
                           DEFAULT_PITCH | FF_DONTCARE,     // family and pitch
                           EDITOR_FONT );                   // font name

    oldFont = ( HFONT ) SelectObject ( hdc, winFont );      // selects the font we created
    wglUseFontBitmaps ( hdc, 0, 256, font );                // builds 256 characters starting at character 0
    SelectObject ( hdc, oldFont );                          // selects the old font again We Want
    DeleteObject ( winFont );                               // delete the font we created
}

void renderText ( const string& str )
{
    if ( str.empty() )
        return;

    glPushAttrib ( GL_LIST_BIT );
    glListBase ( font );
    glCallLists ( str.size(), GL_UNSIGNED_BYTE, str.c_str() );
    glPopAttrib();
}

void setupOpenGL()
{
    setupFont();

    glMatrixMode ( GL_PROJECTION );
    glLoadIdentity();
    glOrtho ( 0, screenWidth, screenHeight, 0, -2048, 2048 );

    glMatrixMode ( GL_MODELVIEW );
    glLoadIdentity();

    glEnable ( GL_BLEND );
    glBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glDisable ( GL_DEPTH_TEST );
}

void displayBg()
{
    glBegin ( GL_QUADS );

    glColor3f ( 0.0, 0.0, 0.0 );
    glVertex2f ( 0.0, 0.0 );
    glVertex2f ( screenWidth, 0.0 );
    glVertex2f ( screenWidth, screenHeight );
    glVertex2f ( 0.0, screenHeight );

    glEnd();
}

void displaySprite()
{
    glPushMatrix();

    glTranslatef ( screenWidth * spriteX, screenHeight * spriteY, 0.0 );

    frameDisp.render ( &renderProps );

    glPopMatrix();
}

void displayText()
{
    glPushMatrix();

    glTranslatef ( 10.0, EDITOR_FONT_HEIGHT, 0.0 );

    glColor3f ( EDITOR_FONT_COLOR );
    glRasterPos2f ( 0.0, 0.0 );

    const uint32_t currColor = 0xFFFFFF & frameDisp.get_palette_data() [palette][color];

    if ( uiMode != ColorHexEntry )
        colorHexStr = format ( "%06X", currColor );

    renderText ( format ( "%s - Palette %d - Color %d - #%s%s",
                          frameDisp.get_character_name ( frameDisp.get_character() ) + 2, // TODO allow moon switching
                          palette + 1,
                          color + 1,
                          colorHexStr,
                          ( ( uiMode != ColorHexEntry ) || ( ( ticker % 40 ) > 20 ) ) ? "" : "_" ) );

    glPopMatrix();
}

void displayScene()
{
    displayBg();
    displaySprite();
    displayText();
}

bool handleNavigationKeys ( const SDL_keysym& keysym )
{
    switch ( keysym.sym )
    {
        case SDLK_PAGEUP:
            // prev char
            color = palette = 0;
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
            return true;

        case SDLK_PAGEDOWN:
            // next char
            color = palette = 0;
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
            return true;

        case SDLK_UP:
            // prev palette
            color = 0;
            palette = ( palette + 35 ) % 36;
            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
            return true;

        case SDLK_DOWN:
            // next palette
            color = 0;
            palette = ( palette + 1 ) % 36;
            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
            return true;

        case SDLK_LEFT:
            // prev color
            color = ( color + 255 ) % 256;
            return true;

        case SDLK_RIGHT:
            // next color
            color = ( color + 1 ) % 256;
            return true;

        case SDLK_v:
            if ( keysym.mod & KMOD_CTRL )
            {
                string clip = trimmed ( getClipboard() );

                if ( !clip.empty() && clip[0] == '#' )
                {
                    colorHexStr = clip.substr ( 1, 6 );

                    for ( char& c : colorHexStr )
                        c = toupper ( c );

                    uiMode = ColorHexEntry;
                }
            }
            break;

        case SDLK_BACKSPACE:
            colorHexStr = format ( "%06X", 0xFFFFFF & frameDisp.get_palette_data() [palette][color] );
            colorHexStr.pop_back();
            uiMode = ColorHexEntry;
            break;

        default:
            break;
    }

    return false;
}

bool handleCharacterEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

bool handlePaletteEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

bool handleColorNumEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

bool handleColorHexEntryKeys ( const SDL_keysym& keysym )
{
    switch ( keysym.sym )
    {
        case SDLK_0:
        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
        case SDLK_4:
        case SDLK_5:
        case SDLK_6:
        case SDLK_7:
        case SDLK_8:
        case SDLK_9:
        case SDLK_a:
        case SDLK_b:
        case SDLK_c:
        case SDLK_d:
        case SDLK_e:
        case SDLK_f:
            if ( colorHexStr.size() < 6 )
                colorHexStr.push_back ( toupper ( ( char ) keysym.unicode ) );
            break;

        case SDLK_v:
            if ( ( keysym.mod & KMOD_CTRL ) && colorHexStr.size() < 6 )
            {
                string clip = trimmed ( getClipboard() );

                if ( !clip.empty() && clip[0] == '#' )
                    clip = clip.substr ( 1 );

                colorHexStr += clip.substr ( 0, 6 - colorHexStr.size() );

                for ( char& c : colorHexStr )
                    c = toupper ( c );
            }
            break;

        case SDLK_BACKSPACE:
            if ( !colorHexStr.empty() )
                colorHexStr.pop_back();
            break;

        case SDLK_RETURN:
            if ( colorHexStr.empty() )
            {
                uiMode = Navigation;
                break;
            }

            while ( colorHexStr.size() < 6 )
                colorHexStr.push_back ( colorHexStr.back() );

            for ( char& c : colorHexStr )
            {
                c = toupper ( c );

                if ( ! ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) ) )
                {
                    uiMode = Navigation;
                    return false;
                }
            }

            if ( colorHexStr.size() == 6 )
            {
                uint32_t& currColor = frameDisp.get_palette_data() [palette][color];
                currColor = ( currColor & 0xFF000000 ) | parseHex<uint32_t> ( colorHexStr );

                uiMode = Navigation;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

int main ( int argc, char *argv[] )
{
    SDL_Init ( SDL_INIT_VIDEO | SDL_INIT_TIMER );

    SDL_GL_SetAttribute ( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute ( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_Surface *screen = SDL_SetVideoMode ( screenWidth, screenHeight, 0, SDL_OPENGL | SDL_RESIZABLE );

    if ( !screen )
        return -1;

    SDL_EnableUNICODE ( 1 );
    SDL_EnableKeyRepeat ( 300, 50 );
    SDL_WM_SetCaption ( "Palette Editor", 0 );

    setupOpenGL();

    if ( argc >= 2 && !frameDisp.init ( ( string ( argv[1] ) + "\\0002.p" ).c_str() ) )
        return -1;

    if ( !frameDisp.init() )
        return -1;

    bool done = false, render = false, animate = true;

    while ( !done )
    {
        bool changed = false;

        if ( animate )
        {
            frameDisp.command ( COMMAND_SUBFRAME_NEXT, 0 );
            render = true;
        }

        if ( render )
        {
            displayScene();
            SDL_GL_SwapBuffers();
            render = false;
        }

        ++ticker;

        SDL_Delay ( EDITOR_FRAME_INTERVAL );

        SDL_Event event;
        SDL_PumpEvents();

        while ( SDL_PollEvent ( &event ) )
        {
            switch ( event.type )
            {
                case SDL_KEYDOWN:
                    if ( ( event.key.keysym.mod & KMOD_ALT ) && ( event.key.keysym.sym == SDLK_F4 ) )
                    {
                        done = true;
                        break;
                    }

                    if ( event.key.keysym.sym == SDLK_ESCAPE )
                    {
                        uiMode = Navigation;
                        break;
                    }

                    if ( event.key.keysym.sym == SDLK_SPACE )
                    {
                        animate = !animate;
                        break;
                    }

                    if ( event.key.keysym.unicode == '#' )
                    {
                        colorHexStr.clear();
                        uiMode = ColorHexEntry;
                        break;
                    }

                    switch ( uiMode )
                    {
                        default:
                        case Navigation:
                            changed = handleNavigationKeys ( event.key.keysym );
                            break;

                        case CharacterEntry:
                            changed = handleCharacterEntryKeys ( event.key.keysym );
                            break;

                        case PaletteEntry:
                            changed = handlePaletteEntryKeys ( event.key.keysym );
                            break;

                        case ColorNumEntry:
                            changed = handleColorNumEntryKeys ( event.key.keysym );
                            break;

                        case ColorHexEntry:
                            changed = handleColorHexEntryKeys ( event.key.keysym );
                            break;
                    }
                    break;

                case SDL_VIDEOEXPOSE:
                    changed = true;
                    break;

                case SDL_VIDEORESIZE:
                    SDL_SetVideoMode ( event.resize.w, event.resize.h, 0, SDL_OPENGL | SDL_RESIZABLE );
                    glViewport ( 0, 0, event.resize.w, event.resize.h );

                    screenWidth = event.resize.w;
                    screenHeight = event.resize.h;

                    setupOpenGL();

                    frameDisp.flush_texture();
                    changed = true;
                    break;

                case SDL_QUIT:
                    done = 1;
                    break;
            }
        }

        if ( changed )
        {
            render = true;
        }
    }

    releaseFont();

    frameDisp.free();

    SDL_Quit();

    return 0;
}
