#include "PaletteEditor.h"
#include "Algorithms.h"
#include "KeyValueStore.h"

#include <AntTweakBar.h>

#include <gl.h>
#include <glu.h>
#include <glfw.h>

#include <windows.h>
#include <direct.h>

#include <cstdarg>
#include <climits>
#include <cmath>
#include <string>
#include <algorithm>
#include <unordered_map>

using namespace std;


#define WINDOW_TITLE            "Palette Editor"

#define DATA_FILE               "0002.p"

#define CONFIG_FILE             "palettes.ini"

#define DEFAULT_SPRITE_X        ( 0.5 )

#define DEFAULT_SPRITE_Y        ( 0.7 )


static int screenWidth = 640, screenHeight = 480;

static bool animation = true, highlight = true;

static double spriteX = DEFAULT_SPRITE_X, spriteY = DEFAULT_SPRITE_Y;

static double zoom = 1.0;

static KeyValueStore config;

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
GET_SET_PAIR ( SpriteFrame, 1 )

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

static void TW_CALL getHighlight ( void *value, void *clientData )
{
    * ( bool * ) value = highlight;
}

static void TW_CALL setHighlight ( const void *value, void *clientData )
{
    highlight = * ( bool * ) value;

    if ( highlight )
        editor.ticker = 0;
}

static void cachedEditorState()
{
    config.setInteger ( "screenWidth", screenWidth );
    config.setInteger ( "screenHeight", screenHeight );
    config.setInteger ( "animation", animation );
    config.setInteger ( "highlight", highlight );

    config.setDouble ( "spriteX", spriteX );
    config.setDouble ( "spriteY", spriteY );
    config.setDouble ( "zoom", zoom );

    config.setInteger ( "lastCharacter",   editor.getChara()         + 1 );
    config.setInteger ( "lastPalette",     editor.getPaletteNumber() + 1 );
    config.setInteger ( "lastColorNumber", editor.getColorNumber()   + 1 );
    config.setInteger ( "lastSprite",      editor.getSpriteNumber()  + 1 );
    config.setInteger ( "lastFrame",       editor.getSpriteFrame()   + 1 );
}

static void loadEditorState()
{
    cachedEditorState();

    config.load ( CONFIG_FILE );

    screenWidth  = config.getInteger ( "screenWidth" );
    screenHeight = config.getInteger ( "screenHeight" );
    animation    = config.getInteger ( "animation" );
    highlight    = config.getInteger ( "highlight" );

    spriteX      = config.getDouble ( "spriteX" );
    spriteY      = config.getDouble ( "spriteY" );
    zoom         = config.getDouble ( "zoom" );

    editor.setChara         ( config.getInteger ( "lastCharacter" )   - 1 );
    editor.setPaletteNumber ( config.getInteger ( "lastPalette" )     - 1 );
    editor.setColorNumber   ( config.getInteger ( "lastColorNumber" ) - 1 );
    editor.setSpriteNumber  ( config.getInteger ( "lastSprite" )      - 1 );
    editor.setSpriteFrame   ( config.getInteger ( "lastFrame" )      - 1 );
}

static void saveEditorState()
{
    cachedEditorState();

    config.save ( CONFIG_FILE );
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

int GLFWCALL glfwMouseMoveEvent ( int mouseX, int mouseY )
{
    if ( TwEventMousePosGLFW ( mouseX, mouseY ) )
        return 1;

    static int lastX = INT_MIN, lastY = INT_MIN;

    int handled = 0;

    if ( glfwGetMouseButton ( GLFW_MOUSE_BUTTON_LEFT ) && lastX != INT_MIN && lastY != INT_MIN )
    {
        const int deltaX = mouseX - lastX, deltaY = mouseY - lastY;

        spriteX = ( screenWidth * spriteX + deltaX ) / screenWidth;
        spriteY = ( screenHeight * spriteY + deltaY ) / screenHeight;

        handled = 1;
    }

    lastX = mouseX;
    lastY = mouseY;

    return handled;
}

int GLFWCALL glfwMouseWheelEvent ( int wheelPos )
{
    if ( TwEventMouseWheelGLFW ( wheelPos ) )
        return 1;

    static int lastPos = INT_MIN;

    int handled = 0;

    if ( lastPos != INT_MIN )
    {
        const int deltaPos = wheelPos - lastPos;

        if ( deltaPos < 0 )
            zoom = clamped ( zoom * pow ( 0.95, ( double ) - deltaPos ), 1.0, 10.0 );
        else
            zoom = clamped ( zoom * pow ( 1.05, ( double ) deltaPos ), 1.0, 10.0 );

        handled = 1;
    }

    lastPos = wheelPos;

    return handled;
}

void GLFWCALL glfwResizeWindow ( int width, int height )
{
    glViewport ( 0, 0, width, height );

    glMatrixMode ( GL_PROJECTION );
    glLoadIdentity();
    glOrtho ( 0, width, height, 0, -2048, 2048 );

    TwWindowSize ( width, height );

    screenWidth = width;
    screenHeight = height;
}

void GLFWCALL glfwRefreshWindow()
{
    int width, height;
    glfwGetWindowSize ( &width, &height );
    glfwResizeWindow ( width, height );
}


static TwBar *savePrompt = 0;

static int targetPalette = 0;


static void TW_CALL getTargetPalette ( void *value, void *clientData )
{
    * ( int * ) value = targetPalette + 1;
}

static void TW_CALL setTargetPalette ( const void *value, void *clientData )
{
    targetPalette = * ( int * ) value - 1;
}

static void TW_CALL savePromptSaveButton ( void *clientData )
{
    editor.save ( targetPalette );

    TwDeleteBar ( savePrompt );
    savePrompt = 0;
}

static void TW_CALL savePromptCancelButtom ( void *clientData )
{
    TwDeleteBar ( savePrompt );
    savePrompt = 0;
}

static void TW_CALL saveCurrentColorPrompt ( void *clientData )
{
    if ( savePrompt )
    {
        TwDefine ( " savePrompt iconified=false " );
        return;
    }

    targetPalette = editor.getPaletteNumber();

    savePrompt = TwNewBar ( "savePrompt" );
    TwDefine ( " savePrompt label='Save Current Palette To' position='16 352' size='200 112' color='15 97 127' alpha=64"
               " resizable=false " );

    TwAddVarCB ( savePrompt, "targetPalette", TW_TYPE_INT32, setTargetPalette, getTargetPalette, 0,
                 " label='Palette' " );

    TwAddButton ( savePrompt, "blankSave", 0, 0, " label=' ' " );

    TwAddButton ( savePrompt, "saveButton", savePromptSaveButton, 0,
                  " label='Save' " );

    TwAddButton ( savePrompt, "cancelButton", savePromptCancelButtom, 0,
                  " label='Cancel' " );

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

    loadEditorState();
    saveEditorState();

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
    glfwSetWindowRefreshCallback ( glfwRefreshWindow );

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
    glfwSetMousePosCallback ( ( GLFWmouseposfun ) glfwMouseMoveEvent );
    glfwSetMouseWheelCallback ( ( GLFWmousewheelfun ) glfwMouseWheelEvent );
    glfwSetKeyCallback ( ( GLFWkeyfun ) glfwKeyboardEvent );
    glfwSetCharCallback ( ( GLFWcharfun ) TwEventCharGLFW );

    TwDefine ( " GLOBAL fontsize=3 fontstyle=default fontresizable=false " );

    bar = TwNewBar ( "main" );
    TwDefine ( " main label='Main Options' position='16 16' size='200 320' valueswidth=60"
               " resizable=false movable=false iconifiable=false " );

    vector<TwEnumVal> charas;
    for ( int i = 0; i < editor.getCharaCount(); ++i )
    {
        TwEnumVal val = { i, editor.getCharaName ( i ) };
        charas.push_back ( val );
    }

    TwType charaType = TwDefineEnum ( "CharaType", &charas[0], charas.size() );
    TwAddVarCB ( bar, "character", charaType, setChara, getChara, 0,
                 " label='Character' " );

    TwAddVarCB ( bar, "spriteNumber", TW_TYPE_INT32, setSpriteNumber, getSpriteNumber, 0,
                 " label='Sprite' keydecr=PGUP keyincr=PGDOWN " );

    TwAddVarCB ( bar, "spriteFrame", TW_TYPE_INT32, setSpriteFrame, getSpriteFrame, 0,
                 " label='Frame' keydecr=[ keyincr=] " );

    TwAddVarCB ( bar, "paletteNumber", TW_TYPE_INT32, setPaletteNumber, getPaletteNumber, 0,
                 " label='Palette' keydecr=UP keyincr=DOWN " );

    TwAddVarCB ( bar, "colorNumber", TW_TYPE_INT32, setColorNumber, getColorNumber, 0,
                 " label='Color Number' keydecr=LEFT keyincr=RIGHT " );

    TwAddVarCB ( bar, "originalColor", TW_TYPE_STDSTRING, 0, getOriginalColor, 0,
                 " label='Original Color' " );

    TwAddVarCB ( bar, "currentColor", TW_TYPE_STDSTRING, setCurrentColor, getCurrentColor, 0,
                 " label='Current Color' key=# " );

    TwAddButton ( bar, "blank1", 0, 0, " label=' ' " );

    TwAddVarRW ( bar, "animation", TW_TYPE_BOOLCPP, &animation,
                 " label='Animation' key=F1 " );

    TwAddVarCB ( bar, "highlight", TW_TYPE_BOOLCPP, setHighlight, getHighlight, 0,
                 " label='Highlight' key=F2 " );

    TwAddButton ( bar, "blank2", 0, 0, " label=' ' " );

    TwAddButton ( bar, "save", saveCurrentColorPrompt, 0, " label='Save' key=CTRL+s " );

    // Main loop
    while ( glfwGetWindowParam ( GLFW_OPENED ) )
    {
        // Clear screen
        glClearColor ( 0.0, 0.0, 0.0, 1 );
        glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        // Render sprite
        glPushMatrix();
        glTranslated ( screenWidth * spriteX, screenHeight * spriteY, 0.0 );
        glScaled ( zoom, zoom, 1.0 );

        editor.highlightCurrentColor ( highlight && ( editor.ticker % 60 ) < 30 );

        editor.renderSprite();

        if ( animation )
        {
            editor.nextSpriteSubFrame();
            TwRefreshBar ( bar );
        }

        glPopMatrix();

        // Render UI
        TwDraw();

        glfwSwapBuffers();
        Sleep ( 16 );
        ++editor.ticker;
    }

    saveEditorState();

    TwTerminate();

    glfwTerminate();

    editor.free();

    return 0;
}
