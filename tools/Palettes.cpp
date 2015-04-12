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


static int screenWidth = 640;
static int screenHeight = 480;

static float spriteX = 0.5;
static float spriteY = 0.7;

static PaletteEditor editor;

static TwBar *bar;


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
GET_SET_PAIR ( SpriteNumber, 1 )

static void TW_CALL getOriginalColor ( void *value, void *clientData )
{
    TwCopyStdStringToLibrary ( * ( string * ) value, editor.getOriginalColorHex() );
}

static void TW_CALL getCurrentColor ( void *value, void *clientData )
{
    TwCopyStdStringToLibrary ( * ( string * ) value, editor.getCurrentColorHex() );
}

static void TW_CALL setCurrentColor ( const void *value, void *clientData )
{
    if ( ( ( string * ) value )->empty() )
        editor.clearCurrentColor();
    else
        editor.setCurrentColor ( * ( string * ) value );
}

int GLFWCALL glfwKeyboardEvent ( int key, int action )
{
    if ( TwEventKeyGLFW ( key, action ) )
        return 1;

    if ( action == GLFW_PRESS )
    {
        switch ( key )
        {
            case GLFW_KEY_HOME:
                editor.setSpriteNumber ( 0 );
                TwRefreshBar ( bar );
                return 1;

            case GLFW_KEY_DEL:
                editor.clearCurrentColor();
                TwRefreshBar ( bar );
                return 1;

            default:
                break;
        }
    }

    return 0;
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
    glfwSetKeyCallback ( ( GLFWkeyfun ) glfwKeyboardEvent );
    glfwSetCharCallback ( ( GLFWcharfun ) TwEventCharGLFW );

    TwDefine ( " GLOBAL fontsize=3 fontstyle=default fontresizable=false " );

    bar = TwNewBar ( "Main" );
    TwDefine ( " Main label='Main Options' size='200 320' valueswidth=60"
               " resizable=false movable=false iconifiable=false " );

    vector<TwEnumVal> charas;
    for ( int i = 0; i < editor.getCharaCount(); ++i )
    {
        TwEnumVal val = { i, editor.getCharaName ( i ) };
        charas.push_back ( val );
    }

    TwType charaType = TwDefineEnum ( "CharaType", &charas[0], charas.size() );
    TwAddVarCB ( bar, "Character", charaType, setChara, getChara, 0, "" );

    TwAddVarCB ( bar, "Sprite", TW_TYPE_INT32, setSpriteNumber, getSpriteNumber, 0,
                 " label='Sprite' keyincr=PGDOWN keydecr=PGUP" );

    TwAddVarCB ( bar, "Palette", TW_TYPE_INT32, setPaletteNumber, getPaletteNumber, 0,
                 " label='Palette' min=1, max=36 keyincr=DOWN keydecr=UP" );

    TwAddVarCB ( bar, "ColorNumber", TW_TYPE_INT32, setColorNumber, getColorNumber, 0,
                 " label='Color Number' min=1, max=256 keyincr=RIGHT keydecr=LEFT" );

    TwAddVarCB ( bar, "Original", TW_TYPE_STDSTRING, 0, getOriginalColor, 0,
                 " label='Original Color'" );

    TwAddVarCB ( bar, "Color", TW_TYPE_STDSTRING, setCurrentColor, getCurrentColor, 0,
                 " label='Current Color' key=#" );

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
        Sleep ( 16 );
    }

    TwTerminate();

    glfwTerminate();

    editor.free();

    return 0;
}
