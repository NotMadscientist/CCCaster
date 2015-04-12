#include "PaletteEditor.h"
#include "StringUtils.h"

#include <AntTweakBar.h>

#include <gl.h>
#include <glu.h>
#include <glfw.h>

#include <windows.h>
#include <direct.h>

#include <cstdarg>
#include <string>
#include <algorithm>
#include <unordered_map>

using namespace std;


#define WINDOW_TITLE            "Palette Editor"

#define DATA_FILE               "0002.p"

#define KEY_REPEAT_DELAY        ( 300 )

#define KEY_REPEAT_INTERVAL     ( 50 )

#define HELD_DELETE_TICKS       ( ( 1000 - KEY_REPEAT_DELAY ) / KEY_REPEAT_INTERVAL )

#define EDITOR_FRAME_INTERVAL   ( 1000 / 60 )

#define EDITOR_FONT             "Tahoma"

#define EDITOR_FONT_HEIGHT      ( 16 )

#define EDITOR_FONT_WIDTH       ( 0 )

#define EDITOR_FONT_WEIGHT      ( 600 )

#define EDITOR_FONT_COLOR       1.0, 1.0, 1.0

#define EDITOR_BAD_HEX_DELAY    ( 1000 )


static int screenWidth = 640;
static int screenHeight = 480;

static float spriteX = 0.5;
static float spriteY = 0.7;

// static uint32_t ticker = 0;

// static bool animate = true, highlight = true;

// static int paletteNumber = 0, colorNumber = 0;

// static unordered_map<uint32_t, PaletteManager> palMans;

// static enum { Navigation, CharacterEntry, PaletteEntry, ColorNumEntry, ColorHexEntry, } uiMode = Navigation;

// static string colorHexStr, message, status;


// static uint32_t getChara()
// {
//     return ( uint32_t ) frameDisp.get_character_index ( frameDisp.get_character() );
// }

// static string getCharaName()
// {
//     return getShortCharaName ( getChara() );
// }

// static string getClipboard()
// {
//     const char *buffer = "";

//     if ( OpenClipboard ( 0 ) )
//     {
//         HANDLE hData = GetClipboardData ( CF_TEXT );
//         buffer = ( const char * ) GlobalLock ( hData );
//         if ( buffer == 0 )
//             buffer = "";
//         GlobalUnlock ( hData );
//         CloseClipboard();
//     }

//     return string ( buffer );
// }

// static void setClipboard ( const string& str )
// {
//     if ( OpenClipboard ( 0 ) )
//     {
//         HGLOBAL clipbuffer = GlobalAlloc ( GMEM_DDESHARE, str.size() + 1 );
//         char *buffer = ( char * ) GlobalLock ( clipbuffer );
//         strcpy ( buffer, LPCSTR ( str.c_str() ) );
//         GlobalUnlock ( clipbuffer );
//         EmptyClipboard();
//         SetClipboardData ( CF_TEXT, clipbuffer );
//         CloseClipboard();
//     }
// }

// static void displayText()
// {
//     glPushMatrix();

//     glTranslatef ( 10.0, 5.0 + EDITOR_FONT_HEIGHT, 0.0 );

//     glColor3f ( EDITOR_FONT_COLOR );

//     glRasterPos2f ( 0.0, 0.0 );
//     renderText ( format ( "%s - Palette %d - Color %d", getCharaName(), paletteNumber + 1, colorNumber + 1 ) );

//     const uint32_t origColor = 0xFFFFFF & palMans[getChara()].getOriginal ( paletteNumber, colorNumber );

//     glRasterPos2f ( 0.0, 5.0 + EDITOR_FONT_HEIGHT );
//     renderText ( format ( "Original #%06X", origColor ) );

//     const uint32_t currColor = 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );

//     if ( uiMode != ColorHexEntry )
//         colorHexStr = format ( "%06X", currColor );

//     string blinker = ( ( uiMode != ColorHexEntry ) || ( ( ticker % 40 ) > 20 ) ) ? "" : "_";

//     glRasterPos2f ( 0.0, 2 * ( 5.0 + EDITOR_FONT_HEIGHT ) );
//     renderText ( format ( "Current #%s%s", colorHexStr, blinker ) );

//     glRasterPos2f ( 0.0, 3 * ( 5.0 + EDITOR_FONT_HEIGHT ) );
//     renderText ( message );

//     for ( size_t i = 0; i < controls.size(); ++i )
//     {
//         glRasterPos2f ( 0.0,
//                         screenHeight
//                         - ( 1 + controls.size() - i ) * ( 5.0 + EDITOR_FONT_HEIGHT )
//                         - 2 * EDITOR_FONT_HEIGHT );
//         renderText ( controls[i] );
//     }

//     glRasterPos2f ( 0.0, screenHeight - 2 * EDITOR_FONT_HEIGHT );
//     renderText ( status );

//     glPopMatrix();
// }

// static void savePalMan()
// {
//     if ( palMans.find ( getChara() ) == palMans.end() )
//         return;

//     _mkdir ( PALETTES_FOLDER );
//     palMans[getChara()].save ( PALETTES_FOLDER, getCharaName() );
// }

// static void initPalMan()
// {
//     if ( palMans.find ( getChara() ) == palMans.end() )
//     {
//         palMans[getChara()].cache ( static_cast<const MBAACC_FrameDisplay&> ( frameDisp ).get_palette_data() );
//         palMans[getChara()].load ( PALETTES_FOLDER, getCharaName() );
//     }

//     palMans[getChara()].apply ( frameDisp.get_palette_data() );
// }

// static uint32_t deleteDownTicks = 0;

// static bool isValidColorCode ( const string& str )
// {
//     for ( char c : str )
//     {
//         c = toupper ( c );

//         if ( ! ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) ) )
//         {
//             return false;
//         }
//     }

//     return true;
// }

// static bool handleNavigationKeys ( const SDL_keysym& keysym )
// {
//     // Zero constant for FrameDisplay parameters
//     static const int zero = 0;

//     // Enter color hex entry mode
//     if ( keysym.unicode == '#' )
//     {
//         colorHexStr.clear();
//         uiMode = ColorHexEntry;
//         return false;
//     }

//     switch ( keysym.sym )
//     {
//         // Toggle animation
//         case SDLK_F1:
//             animate = !animate;
//             break;

//         // Toggle highlight
//         case SDLK_F2:
//             highlight = !highlight;
//             return true;

//         // Previous character
//         case SDLK_PAGEUP:
//         case SDLK_KP9:
//             colorNumber = paletteNumber = 0;
//             frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
//             frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
//             frameDisp.command ( COMMAND_CHARACTER_PREV, 0 );
//             frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
//             message.clear();
//             initPalMan();
//             return true;

//         // Next character
//         case SDLK_PAGEDOWN:
//         case SDLK_KP3:
//             colorNumber = paletteNumber = 0;
//             frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
//             frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
//             frameDisp.command ( COMMAND_CHARACTER_NEXT, 0 );
//             frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
//             message.clear();
//             initPalMan();
//             return true;

//         // Previous palette
//         case SDLK_UP:
//         case SDLK_KP8:
//             colorNumber = 0;
//             paletteNumber = ( paletteNumber + 35 ) % 36;
//             frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
//             message.clear();
//             return true;

//         // Next palette
//         case SDLK_DOWN:
//         case SDLK_KP2:
//             colorNumber = 0;
//             paletteNumber = ( paletteNumber + 1 ) % 36;
//             frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
//             message.clear();
//             return true;

//         // Previous color
//         case SDLK_LEFT:
//         case SDLK_KP4:
//             colorNumber = ( colorNumber + 255 ) % 256;
//             message.clear();
//             return true;

//         // Next color
//         case SDLK_RIGHT:
//         case SDLK_KP6:
//             colorNumber = ( colorNumber + 1 ) % 256;
//             message.clear();
//             return true;

//         // First sequence
//         case SDLK_HOME:
//         case SDLK_KP7:
//             frameDisp.command ( COMMAND_SEQUENCE_SET, &zero );
//             break;

//         // Previous sequence
//         case SDLK_MINUS:
//         case SDLK_KP_MINUS:
//             frameDisp.command ( COMMAND_SEQUENCE_PREV, 0 );
//             break;

//         // Next sequence
//         case SDLK_EQUALS:
//         case SDLK_KP_PLUS:
//             frameDisp.command ( COMMAND_SEQUENCE_NEXT, 0 );
//             break;

//         // Copy the current color to clipboard
//         case SDLK_c:
//             if ( keysym.mod & KMOD_CTRL )
//             {
//                 const uint32_t currColor = 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );
//                 const string str = format ( "#%06X", currColor );
//                 setClipboard ( str );
//                 message = "Current color " + str + " copied to clipboard";
//             }
//             break;

//         // Paste a color and enter color hex entry mode
//         case SDLK_v:
//             if ( keysym.mod & KMOD_CTRL )
//             {
//                 string clip = trimmed ( getClipboard() );

//                 if ( !clip.empty() && ( clip[0] == '#' || isValidColorCode ( clip ) ) )
//                 {
//                     if ( clip[0] == '#' )
//                         colorHexStr = clip.substr ( 1, 6 );
//                     else
//                         colorHexStr = clip.substr ( 0, 6 );

//                     for ( char& c : colorHexStr )
//                         c = toupper ( c );

//                     uiMode = ColorHexEntry;
//                 }
//             }
//             break;

//         // Backspace a letter and enter color hex entry mode
//         case SDLK_BACKSPACE:
//         {
//             const uint32_t currColor = 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );
//             colorHexStr = format ( "%06X", currColor );
//             colorHexStr.pop_back();
//             uiMode = ColorHexEntry;
//             break;
//         }

//         default:
//             break;
//     }

//     return false;
// }

// static bool handleColorHexEntryKeys ( const SDL_keysym& keysym )
// {
//     static const string allowedUnicode = "0123456789ABCDEFabcdef";

//     message.clear();

//     switch ( keysym.sym )
//     {
//         // Only allow hex codes
//         case SDLK_0:
//         case SDLK_1:
//         case SDLK_2:
//         case SDLK_3:
//         case SDLK_4:
//         case SDLK_5:
//         case SDLK_6:
//         case SDLK_7:
//         case SDLK_8:
//         case SDLK_9:
//         case SDLK_a:
//         case SDLK_b:
//         case SDLK_c:
//         case SDLK_d:
//         case SDLK_e:
//         case SDLK_f:
//             if ( allowedUnicode.find ( ( char ) keysym.unicode ) == string::npos )
//                 break;
//             if ( colorHexStr.size() < 6 )
//                 colorHexStr.push_back ( toupper ( ( char ) keysym.unicode ) );
//             break;

//         // Paste a color
//         case SDLK_v:
//             if ( ( keysym.mod & KMOD_CTRL ) && colorHexStr.size() < 6 )
//             {
//                 colorHexStr += trimmed ( getClipboard() ).substr ( 0, 6 - colorHexStr.size() );

//                 for ( char& c : colorHexStr )
//                     c = toupper ( c );
//             }
//             break;

//         // Backspace a letter
//         case SDLK_BACKSPACE:
//             if ( !colorHexStr.empty() )
//                 colorHexStr.pop_back();
//             break;

//         // Parse color and return to navigation
//         case SDLK_RETURN:
//         case SDLK_KP_ENTER:
//             if ( colorHexStr.empty() )
//             {
//                 uiMode = Navigation;
//                 break;
//             }

//             while ( colorHexStr.size() < 6 )
//                 colorHexStr.push_back ( colorHexStr.back() );

//             if ( ! isValidColorCode ( colorHexStr ) )
//             {
//                 uiMode = Navigation;
//                 return false;
//             }

//             if ( colorHexStr.size() == 6 )
//             {
//                 const uint32_t newColor = 0xFFFFFF & parseHex<uint32_t> ( colorHexStr );

//                 uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];
//                 currColor = ( currColor & 0xFF000000 ) | newColor;

//                 palMans[getChara()].set ( paletteNumber, colorNumber, newColor );
//                 savePalMan();

//                 uiMode = Navigation;
//                 return true;
//             }
//             break;

//         default:
//             break;
//     }

//     return false;
// }

// static int old ( int argc, char *argv[] )
// {
//     bool initialized = false;

//     if ( argc >= 2 )
//         initialized = frameDisp.init ( ( string ( argv[1] ) + "\\" DATA_FILE ).c_str() );

//     if ( !initialized )
//         initialized = frameDisp.init ( DATA_FILE );

//     if ( !initialized )
//     {
//         MessageBox ( 0, "Could not load palette data!\n\nIs " DATA_FILE " in the same folder?", "Error", MB_OK );
//         return -1;
//     }

//     // Initialize with the first character
//     initPalMan();

//     if ( !setupSDL() )
//     {
//         MessageBox ( 0, "Could not create window!", "Error", MB_OK );
//         return -1;
//     }

//     setupOpenGL();

//     bool done = false;

//     while ( !done )
//     {
//         bool changed = false;

//         if ( animate )
//             frameDisp.command ( COMMAND_SUBFRAME_NEXT, 0 );

//         if ( highlight && ( ticker % 30 == 0 ) )
//         {
//             const uint32_t color = 0xFFFFFF & SWAP_R_AND_B ( palMans[getChara()].get ( paletteNumber, colorNumber ) );
//             const uint32_t highlight = 0xFFFFFF & PaletteManager::computeHighlightColor ( color );

//             uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];

//             if ( ( currColor & 0xFFFFFF ) == color )
//                 currColor = ( currColor & 0xFF000000 ) | highlight;
//             else
//                 currColor = ( currColor & 0xFF000000 ) | color;

//             frameDisp.flush_texture();
//         }

//         status = format ( "Display options: (F1) Animation %s (F2) Highlight %s",
//                           ( animate ? "ON" : "OFF" ),
//                           ( highlight ? "ON" : "OFF" ) );

//         displayScene();
//         SDL_GL_SwapBuffers();

//         ++ticker;

//         SDL_Delay ( EDITOR_FRAME_INTERVAL );

//         SDL_Event event;
//         SDL_PumpEvents();

//         while ( SDL_PollEvent ( &event ) )
//         {
//             if ( TwEventSDL ( &event, SDL_MAJOR_VERSION, SDL_MINOR_VERSION ) )
//                 continue;

//             switch ( event.type )
//             {
//                 case SDL_KEYDOWN:
//                     // Quit application
//                     if ( ( event.key.keysym.mod & KMOD_ALT ) && ( event.key.keysym.sym == SDLK_F4 ) )
//                     {
//                         done = true;
//                         break;
//                     }

//                     // Return to navigation
//                     if ( event.key.keysym.sym == SDLK_ESCAPE )
//                     {
//                         uiMode = Navigation;
//                         break;
//                     }

//                     // Reset current color to the original
//                     if ( ( event.key.keysym.sym == SDLK_DELETE ) || ( event.key.keysym.sym == SDLK_KP_PERIOD ) )
//                     {
//                         uiMode = Navigation;

//                         ++deleteDownTicks;

//                         // Held delete resets all colors of the current palette to the original
//                         if ( deleteDownTicks > HELD_DELETE_TICKS )
//                         {
//                             palMans[getChara()].clear ( paletteNumber );
//                             palMans[getChara()].apply ( frameDisp.get_palette_data() );
//                             savePalMan();

//                             message = "Current palette reset to original colors";
//                             changed = true;
//                             break;
//                         }

//                         const uint32_t oldColor = 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );

//                         palMans[getChara()].clear ( paletteNumber, colorNumber );
//                         savePalMan();

//                         const uint32_t newColor = 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );

//                         uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];
//                         currColor = ( currColor & 0xFF000000 ) | newColor;

//                         if ( oldColor == newColor )
//                             break;

//                         message = format ( "Current color #%06X reset to original #%06X", oldColor, newColor );
//                         changed = true;
//                         break;
//                     }
//                     break;

//                 case SDL_KEYUP:
//                     if ( ( event.key.keysym.sym == SDLK_DELETE ) || ( event.key.keysym.sym == SDLK_KP_PERIOD ) )
//                         deleteDownTicks = 0;
//                     break;

//                 case SDL_VIDEOEXPOSE:
//                     changed = true;
//                     break;

//                 case SDL_VIDEORESIZE:
//                     SDL_SetVideoMode ( event.resize.w, event.resize.h, 0, SDL_OPENGL | SDL_RESIZABLE );
//                     glViewport ( 0, 0, event.resize.w, event.resize.h );

//                     screenWidth = event.resize.w;
//                     screenHeight = event.resize.h;

//                     setupOpenGL();
//                     changed = true;
//                     break;

//                 case SDL_QUIT:
//                     done = 1;
//                     break;
//             }
//         }

//         if ( changed )
//         {
//             ticker = 0;
//             palMans[getChara()].apply ( frameDisp.get_palette_data() );
//             frameDisp.flush_texture();
//         }
//     }

//     frameDisp.free();

//     TwTerminate();

//     SDL_Quit();

//     return 0;
// }


static PaletteEditor editor;

#define GET_SET_PAIR(PROPERTY, OFFSET)                                                      \
    static void TW_CALL get ## PROPERTY ( void *value, void *clientData ) {                 \
        * ( int * ) value = editor.get ## PROPERTY() + OFFSET;                              \
    }                                                                                       \
    static void TW_CALL set ## PROPERTY ( const void *value, void *clientData ) {           \
        editor.set ## PROPERTY ( ( * ( int * ) value ) - OFFSET );                          \
    }

GET_SET_PAIR ( Chara, 0 )
GET_SET_PAIR ( PaletteNumber, 1 )
GET_SET_PAIR ( ColorNumber, 1 )

static string colorValue;

static void TW_CALL getColorValue ( void *value, void *clientData )
{
    TwCopyStdStringToLibrary ( * ( string * ) value, colorValue );
}

static void TW_CALL setColorValue ( const void *value, void *clientData )
{
    colorValue = * ( string * ) value;
}

void GLFWCALL glfwResizeWindow ( int width, int height )
{
    glViewport ( 0, 0, width, height );

    glMatrixMode ( GL_PROJECTION );
    glLoadIdentity();
    glOrtho ( 0, screenWidth, screenHeight, 0, -2048, 2048 );

    TwWindowSize ( width, height );

    screenWidth = width;
    screenHeight = height;
}

int main ( int argc, char *argv[] )
{
    // Init PaletteEditor
    bool initialized = false;

    if ( argc >= 2 )
        initialized = editor.init ( PALETTES_FOLDER, string ( argv[1] ) + "\\" DATA_FILE );

    if ( !initialized )
        initialized = editor.init ( PALETTES_FOLDER, DATA_FILE );

    if ( !initialized )
    {
        MessageBox ( 0, "Could not load palette data!\n\nIs " DATA_FILE " in the same folder?", "Error", MB_OK );
        return -1;
    }

    // Init GLFW
    if ( !glfwInit() )
    {
        MessageBox ( 0, "Could not initialize OpenGL!", "Error", MB_OK );
        return -1;
    }

    GLFWvidmode videoMode;
    glfwGetDesktopMode ( &videoMode );

    if ( !glfwOpenWindow ( screenWidth, screenHeight,
                           videoMode.RedBits, videoMode.GreenBits, videoMode.BlueBits,
                           0, 16, 0, GLFW_WINDOW ) )
    {
        MessageBox ( 0, "Could not create window!", "Error", MB_OK );
        glfwTerminate();
        return -1;
    }

    glfwEnable ( GLFW_MOUSE_CURSOR );
    glfwEnable ( GLFW_KEY_REPEAT );
    glfwSetWindowTitle ( WINDOW_TITLE );
    glfwSetWindowSizeCallback ( glfwResizeWindow );

    // Init OpenGL
    glMatrixMode ( GL_PROJECTION );
    glLoadIdentity();
    glOrtho ( 0, screenWidth, screenHeight, 0, -2048, 2048 );

    glMatrixMode ( GL_MODELVIEW );
    glLoadIdentity();

    glEnable ( GL_BLEND );
    glBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glDisable ( GL_DEPTH_TEST );

    // Init AntTweakBar
    TwInit ( TW_OPENGL, 0 );

    TwWindowSize ( screenWidth, screenHeight );

    glfwSetMouseButtonCallback ( ( GLFWmousebuttonfun ) TwEventMouseButtonGLFW );
    glfwSetMousePosCallback ( ( GLFWmouseposfun ) TwEventMousePosGLFW );
    glfwSetMouseWheelCallback ( ( GLFWmousewheelfun ) TwEventMouseWheelGLFW );
    glfwSetKeyCallback ( ( GLFWkeyfun ) TwEventKeyGLFW );
    glfwSetCharCallback ( ( GLFWcharfun ) TwEventCharGLFW );

    TwDefine ( " GLOBAL fontsize=3 fontstyle=fixed " );

    TwBar *bar = TwNewBar ( "Main" );
    TwDefine ( " Main label='Main Options' size='230 320' resizable=false movable=false iconifiable=false " );

    vector<TwEnumVal> charas;
    for ( int i = 0; i < editor.getCharaCount(); ++i )
    {
        TwEnumVal val = { i, editor.getCharaName ( i ) };
        charas.push_back ( val );
    }

    TwType charaType = TwDefineEnum ( "CharaType", &charas[0], charas.size() );
    TwAddVarCB ( bar, "Character", charaType, setChara, getChara, 0, "" );

    TwAddVarCB ( bar, "Palette", TW_TYPE_INT32, setPaletteNumber, getPaletteNumber, 0,
                 " label='Palette' min=1, max=36 keyincr=DOWN keydecr=UP" );

    TwAddVarCB ( bar, "ColorNumber", TW_TYPE_INT32, setColorNumber, getColorNumber, 0,
                 " label='Color Number' min=1, max=256 keyincr=RIGHT keydecr=LEFT" );

    TwAddVarCB ( bar, "ColorValue", TW_TYPE_STDSTRING, setColorValue, getColorValue, 0,
                 " label='Color Value' key=#" );

    // Main loop
    while ( glfwGetWindowParam ( GLFW_OPENED ) )
    {
        // Clear screen
        glClearColor ( 0.0, 0.0, 0.0, 1 );
        glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        // Render sprite
        glPushMatrix();
        glTranslatef ( screenWidth * spriteX, screenHeight * spriteY, 0.0 );
        editor.renderSprite();
        editor.nextSpriteFrame();
        glPopMatrix();

        // Render UI
        TwDraw();

        glfwSwapBuffers();
    }

    TwTerminate();

    glfwTerminate();

    editor.free();

    return 0;
}
