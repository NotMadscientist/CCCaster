#include "PaletteManager.h"
#include "StringUtils.h"
#include "CharacterSelect.h"

#include "mbaacc_framedisplay.h"
#include "render.h"

#include <SDL.h>

#include <gl.h>
#include <glext.h>
#include <glu.h>

#include <windows.h>
#include <direct.h>

#include <cstdarg>
#include <string>
#include <algorithm>
#include <unordered_map>

using namespace std;


#define WINDOW_TITLE            "Palette Editor"

#define DATA_FILE               "0002.p"

#define EDITOR_FRAME_INTERVAL   ( 16 )

#define EDITOR_FONT             "Tahoma"

#define EDITOR_FONT_HEIGHT      ( 20 )

#define EDITOR_FONT_WIDTH       ( 0 )

#define EDITOR_FONT_WEIGHT      ( 600 )

#define EDITOR_FONT_COLOR       1.0, 1.0, 1.0

#define EDITOR_BAD_HEX_DELAY    ( 1000 )


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

static int colorNumber = 0, paletteNumber = 0;

static unordered_map<uint32_t, PaletteManager> palMans;

static enum { Navigation, CharacterEntry, PaletteEntry, ColorNumEntry, ColorHexEntry, } uiMode = Navigation;

static string colorHexStr;

static string message;


static uint32_t getChara()
{
    return ( uint32_t ) frameDisp.get_character_index ( frameDisp.get_character() );
}

static string getCharaName()
{
    return getShortCharaName ( getChara() );
}

static string getClipboard()
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

static bool setupSDL()
{
    SDL_Init ( SDL_INIT_VIDEO | SDL_INIT_TIMER );

    SDL_GL_SetAttribute ( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute ( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_Surface *screen = SDL_SetVideoMode ( screenWidth, screenHeight, 0, SDL_OPENGL | SDL_RESIZABLE );

    if ( !screen )
        return false;

    SDL_EnableUNICODE ( 1 );
    SDL_EnableKeyRepeat ( 300, 50 );
    SDL_WM_SetCaption ( WINDOW_TITLE, 0 );

    return true;
}

static void releaseFont()
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

static void setupFont()
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

static void renderText ( const string& str )
{
    if ( str.empty() )
        return;

    glPushAttrib ( GL_LIST_BIT );
    glListBase ( font );
    glCallLists ( str.size(), GL_UNSIGNED_BYTE, str.c_str() );
    glPopAttrib();
}

static void setupOpenGL()
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

static void displayBg()
{
    glBegin ( GL_QUADS );

    glColor3f ( 0.0, 0.0, 0.0 );
    glVertex2f ( 0.0, 0.0 );
    glVertex2f ( screenWidth, 0.0 );
    glVertex2f ( screenWidth, screenHeight );
    glVertex2f ( 0.0, screenHeight );

    glEnd();
}

static void displaySprite()
{
    glPushMatrix();

    glTranslatef ( screenWidth * spriteX, screenHeight * spriteY, 0.0 );

    frameDisp.render ( &renderProps );

    glPopMatrix();
}

static void displayText()
{
    glPushMatrix();

    glTranslatef ( 10.0, 5.0 + EDITOR_FONT_HEIGHT, 0.0 );

    glColor3f ( EDITOR_FONT_COLOR );

    glRasterPos2f ( 0.0, 0.0 );
    renderText ( format ( "%s - Palette %d - Color %d", getCharaName(), paletteNumber + 1, colorNumber + 1 ) );

    const uint32_t origColor = 0xFFFFFF & palMans[getChara()].getOriginal ( paletteNumber, colorNumber );

    glRasterPos2f ( 0.0, 5.0 + EDITOR_FONT_HEIGHT );
    renderText ( format ( "Original #%06X", origColor ) );

    const uint32_t currColor = 0xFFFFFF & frameDisp.get_palette_data() [paletteNumber][colorNumber];

    if ( uiMode != ColorHexEntry )
        colorHexStr = format ( "%06X", SWAP_R_AND_B ( currColor ) );

    string blinker = ( ( uiMode != ColorHexEntry ) || ( ( ticker % 40 ) > 20 ) ) ? "" : "_";

    glRasterPos2f ( 0.0, 2 * ( 5.0 + EDITOR_FONT_HEIGHT ) );
    renderText ( format ( "Current #%s%s", colorHexStr, blinker ) );

    glRasterPos2f ( 0.0, 3 * ( 5.0 + EDITOR_FONT_HEIGHT ) );
    renderText ( message );

    glPopMatrix();
}

static void displayScene()
{
    displayBg();
    displaySprite();
    displayText();
}

static void savePalMan()
{
    if ( palMans.find ( getChara() ) == palMans.end() || palMans[getChara()].empty() )
        return;

    _mkdir ( PALETTES_FOLDER );
    palMans[getChara()].save ( PALETTES_FOLDER, getCharaName() );
}

static void initPalMan()
{
    if ( palMans.find ( getChara() ) == palMans.end() )
    {
        palMans[getChara()].cache ( static_cast<const MBAACC_FrameDisplay&> ( frameDisp ).get_palette_data() );
        palMans[getChara()].load ( PALETTES_FOLDER, getCharaName() );
        savePalMan();
    }

    palMans[getChara()].apply ( frameDisp.get_palette_data() );
}

static bool handleNavigationKeys ( const SDL_keysym& keysym )
{
    static const int zero = 0;

    switch ( keysym.sym )
    {
        // Previous character
        case SDLK_PAGEUP:
            colorNumber = paletteNumber = 0;
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
            frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
            initPalMan();
            return true;

        // Next character
        case SDLK_PAGEDOWN:
            colorNumber = paletteNumber = 0;
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
            frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
            initPalMan();
            return true;

        // Previous palette
        case SDLK_UP:
            colorNumber = 0;
            paletteNumber = ( paletteNumber + 35 ) % 36;
            frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
            return true;

        // Next palette
        case SDLK_DOWN:
            colorNumber = 0;
            paletteNumber = ( paletteNumber + 1 ) % 36;
            frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
            return true;

        // Previous color
        case SDLK_LEFT:
            colorNumber = ( colorNumber + 255 ) % 256;
            return true;

        // Next color
        case SDLK_RIGHT:
            colorNumber = ( colorNumber + 1 ) % 256;
            return true;

        // First sequence
        case SDLK_HOME:
            frameDisp.command ( COMMAND_SEQUENCE_SET, &zero );
            return true;

        // Next sequence
        case SDLK_END:
            frameDisp.command ( COMMAND_SEQUENCE_NEXT, 0 );
            return true;

        // Paste a color and enter color hex entry mode
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

        // Backspace a letter and enter color hex entry mode
        case SDLK_BACKSPACE:
        {
            const uint32_t currColor = 0xFFFFFF & frameDisp.get_palette_data() [paletteNumber][colorNumber];
            colorHexStr = format ( "%06X", SWAP_R_AND_B ( currColor ) );
            colorHexStr.pop_back();
            uiMode = ColorHexEntry;
            break;
        }

        default:
            break;
    }

    return false;
}

static bool handleCharacterEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

static bool handlePaletteEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

static bool handleColorNumEntryKeys ( const SDL_keysym& keysym )
{
    return false;
}

static bool handleColorHexEntryKeys ( const SDL_keysym& keysym )
{
    switch ( keysym.sym )
    {
        // Only allow hex letter
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

        // Paste a color
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

        // Backspace a letter
        case SDLK_BACKSPACE:
            if ( !colorHexStr.empty() )
                colorHexStr.pop_back();
            break;

        // Parse color and return to navigation
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
                const uint32_t newColor = 0xFFFFFF & parseHex<uint32_t> ( colorHexStr );

                uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];
                currColor = ( currColor & 0xFF000000 ) | SWAP_R_AND_B ( newColor );

                palMans[getChara()].set ( paletteNumber, colorNumber, newColor );
                savePalMan();

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
    bool initialized = false;

    if ( argc >= 2 )
        initialized = frameDisp.init ( ( string ( argv[1] ) + "\\" DATA_FILE ).c_str() );

    if ( !initialized )
        initialized = frameDisp.init ( DATA_FILE );

    if ( !initialized )
    {
        MessageBox ( 0, "Could not load palette data!\n\nIs " DATA_FILE " in the same folder?", "Error", MB_OK );
        return -1;
    }

    // Initialize with the first character
    initPalMan();

    if ( !setupSDL() )
    {
        MessageBox ( 0, "Could not create window!", "Error", MB_OK );
        return -1;
    }

    setupOpenGL();

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
                    // Quit application
                    if ( ( event.key.keysym.mod & KMOD_ALT ) && ( event.key.keysym.sym == SDLK_F4 ) )
                    {
                        done = true;
                        break;
                    }

                    // Return to navigation
                    if ( event.key.keysym.sym == SDLK_ESCAPE )
                    {
                        uiMode = Navigation;
                        break;
                    }

                    // Toggle animation
                    if ( event.key.keysym.sym == SDLK_SPACE )
                    {
                        animate = !animate;
                        break;
                    }

                    // Reset current color
                    if ( event.key.keysym.sym == SDLK_DELETE )
                    {
                        uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];
                        palMans[getChara()].clear ( paletteNumber, colorNumber );
                        currColor = SWAP_R_AND_B ( palMans[getChara()].get ( paletteNumber, colorNumber ) );
                        savePalMan();

                        uiMode = Navigation;
                        changed = true;
                        break;
                    }

                    // Enter color hex entry mode
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
