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

using namespace std;


#define EDITOR_FONT             "Tahoma"

#define EDITOR_FONT_HEIGHT      ( 30 )

#define EDITOR_FONT_WIDTH       ( 0 )

#define EDITOR_FONT_WEIGHT      ( 600 )

#define EDITOR_FONT_COLOR       1.0, 1.0, 1.0


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

static int color = 0, palette = 0;


void swapColorKeepAlpha ( unsigned int& a, unsigned int& b )
{
    const unsigned int tmp = a;
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

int renderText ( const char *format, ... )
{
    char buffer[4096];
    va_list args;

    if ( !format )
        return 0;

    va_start ( args, format );
    const int count = vsnprintf ( buffer, sizeof ( buffer ), format, args );
    va_end ( args );

    glPushAttrib ( GL_LIST_BIT );
    glListBase ( font );
    glCallLists ( count, GL_UNSIGNED_BYTE, buffer );
    glPopAttrib();

    return count;
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

    const unsigned int currColor = 0xFFFFFF & frameDisp.get_palette_data() [palette][color];

    renderText ( "%s - Palette %d - Color %d - #%06X",
                 frameDisp.get_character_name ( frameDisp.get_character() ) + 2, // TODO allow moon switching
                 palette + 1,
                 color + 1,
                 currColor );

    glPopMatrix();
}

void displayScene()
{
    displayBg();
    displaySprite();
    displayText();
}

int main ( int argc, char *argv[] )
{
    SDL_Init ( SDL_INIT_VIDEO | SDL_INIT_TIMER );

    SDL_GL_SetAttribute ( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute ( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_Surface *screen = SDL_SetVideoMode ( screenWidth, screenHeight, 0, SDL_OPENGL | SDL_RESIZABLE );

    if ( !screen )
        return -1;

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

        SDL_Delay ( 16 );

        SDL_Event event;
        SDL_PumpEvents();

        while ( SDL_PollEvent ( &event ) )
        {
            switch ( event.type )
            {
                case SDL_KEYDOWN:
                    switch ( event.key.keysym.sym )
                    {
                        case SDLK_ESCAPE:
                            done = true;
                            break;

                        case SDLK_SPACE:
                            animate = !animate;
                            break;

                        // case SDLK_UP:
                        //     // prev seq
                        //     frameDisp.command ( COMMAND_SEQUENCE_PREV, 0 );
                        //     changed = true;
                        //     break;

                        // case SDLK_DOWN:
                        //     // next seq
                        //     frameDisp.command ( COMMAND_SEQUENCE_NEXT, 0 );
                        //     changed = true;
                        //     break;

                        case SDLK_PAGEUP:
                            // prev char
                            color = palette = 0;
                            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
                            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
                            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
                            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
                            changed = true;
                            break;

                        case SDLK_PAGEDOWN:
                            // next char
                            color = palette = 0;
                            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
                            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
                            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
                            frameDisp.command ( COMMAND_PALETTE_SET, &palette );
                            changed = true;
                            break;

                        case SDLK_LEFT:
                            // prev color
                            color = ( color + 255 ) % 256;
                            changed = true;
                            break;

                        case SDLK_RIGHT:
                            // next color
                            color = ( color + 1 ) % 256;
                            changed = true;
                            break;

                        case SDLK_TAB:
                            // next palette
                            palette = ( palette + 1 ) % 36;
                            frameDisp.command ( COMMAND_PALETTE_NEXT, 0 );
                            changed = true;
                            break;

                        default:
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
