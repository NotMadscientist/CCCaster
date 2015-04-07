#include "mbaacc_framedisplay.h"
#include "render.h"

#include <SDL.h>

#include <gl.h>
#include <glext.h>

#include <string>

using namespace std;


static MBAACC_FrameDisplay fdisp;

static int window_width = 640;
static int window_height = 480;

static float window_scale = 1.0;

static int position_x = 250;
static int position_y = 400;

static const RenderProperties props = { 1, 0, 0, 0, 0, 0 };


static void setup_opengl()
{
    glMatrixMode ( GL_PROJECTION );
    glLoadIdentity();
    glOrtho ( 0, window_width, window_height, 0, -2048, 2048 );

    glMatrixMode ( GL_MODELVIEW );
    glLoadIdentity();

    glEnable ( GL_BLEND );
    glBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glDisable ( GL_DEPTH_TEST );
}

static void display_bg()
{
    glBegin ( GL_QUADS );
    glColor4f ( 0.0, 0.0, 0.0, 1.0 );
    glVertex2f ( 0.0, 0.0 );
    glVertex2f ( window_width, 0.0 );
    glVertex2f ( window_width, window_height );
    glVertex2f ( 0.0, window_height );
    glEnd();
}

static void display_scene()
{
    display_bg();

    glPushMatrix();
    glTranslatef ( position_x, position_y, 0.0 );
    glScalef ( window_scale, window_scale, 1.0 );

    fdisp.render ( &props );

    glPopMatrix();
}

int main ( int argc, char *argv[] )
{
    if ( argc >= 2 && !fdisp.init ( ( string ( argv[1] ) + "/0002.p" ).c_str() ) )
        return 0;

    if ( !fdisp.init() )
        return 0;

    SDL_Init ( SDL_INIT_VIDEO | SDL_INIT_TIMER );

    SDL_GL_SetAttribute ( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute ( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_Surface *surface = SDL_SetVideoMode ( 640, 480, 0, SDL_OPENGL | SDL_RESIZABLE );

    if ( !surface )
        return -1;

    SDL_EnableKeyRepeat ( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );

    SDL_WM_SetCaption ( "Palette Editor", 0 );

    setup_opengl();

    SDL_GL_SwapBuffers();

    int animate = 1;
    bool done = false, render = false;

    while ( !done )
    {
        if ( animate )
        {
            fdisp.command ( COMMAND_SUBFRAME_NEXT, 0 );
            render = 1;
        }

        if ( render )
        {
            display_scene();
            SDL_GL_SwapBuffers();

            render = 0;
        }

        SDL_Delay ( 16 );

        SDL_Event sdl_event;
        SDL_PumpEvents();

        while ( SDL_PollEvent ( &sdl_event ) )
        {
            switch ( sdl_event.type )
            {
                case SDL_KEYDOWN:
                    switch ( sdl_event.key.keysym.sym )
                    {
                        case SDLK_ESCAPE:
                            done = 1;
                            break;

                        case SDLK_SPACE:
                            animate = 1 - animate;
                            break;

                        case SDLK_UP:
                        case SDLK_KP8:
                            // prev seq
                            fdisp.command ( COMMAND_SEQUENCE_PREV, 0 );
                            render = 1;
                            break;

                        case SDLK_DOWN:
                        case SDLK_KP2:
                            // next seq
                            fdisp.command ( COMMAND_SEQUENCE_NEXT, 0 );
                            render = 1;
                            break;

                        case SDLK_LEFT:
                        case SDLK_KP4:
                            // prev frame
                            fdisp.command ( COMMAND_FRAME_PREV, 0 );
                            render = 1;
                            break;

                        case SDLK_RIGHT:
                        case SDLK_KP6:
                            // next frame
                            fdisp.command ( COMMAND_FRAME_NEXT, 0 );
                            render = 1;
                            break;

                        case SDLK_PAGEUP:
                        case SDLK_KP9:
                            // prev char
                            fdisp.command ( COMMAND_CHARACTER_PREV, 0 );
                            render = 1;
                            break;

                        case SDLK_PAGEDOWN:
                        case SDLK_KP3:
                            // next char
                            fdisp.command ( COMMAND_CHARACTER_NEXT, 0 );
                            render = 1;
                            break;

                        case SDLK_MINUS:
                        case SDLK_KP_MINUS:
                            // scale
                            window_scale = window_scale / 1.1;
                            render = 1;
                            break;

                        case SDLK_PLUS:
                        case SDLK_EQUALS:
                        case SDLK_KP_PLUS:
                            window_scale = window_scale * 1.1;
                            render = 1;
                            break;

                        case SDLK_BACKSPACE:
                            window_scale = 1.0;
                            render = 1;
                            break;

                        case SDLK_TAB:
                            // flush textures
                            fdisp.command ( COMMAND_PALETTE_NEXT, 0 );

                            render = 1;
                            break;

                        default:
                            break;
                    }
                    break;

                case SDL_VIDEOEXPOSE:
                    render = 1;
                    break;

                case SDL_VIDEORESIZE:
                    SDL_SetVideoMode ( sdl_event.resize.w, sdl_event.resize.h, 0,
                                       SDL_OPENGL | SDL_RESIZABLE );
                    glViewport ( 0, 0, sdl_event.resize.w, sdl_event.resize.h );

                    window_width = sdl_event.resize.w;
                    window_height = sdl_event.resize.h;

                    setup_opengl();

                    // flush textures
                    break;

                case SDL_QUIT:
                    done = 1;
                    break;
            }
        }
    }

    fdisp.free();

    SDL_Quit();

    return 0;
}
