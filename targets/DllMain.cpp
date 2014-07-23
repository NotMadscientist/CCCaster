#include "Logger.h"
#include "Utilities.h"
#include "EventManager.h"
#include "TimerManager.h"
#include "SocketManager.h"
#include "ControllerManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Timer.h"
#include "Thread.h"
#include "AsmHacks.h"
#include "D3DHook.h"
#include "Messages.h"

#include <windows.h>
#include <d3dx9.h>
#include <MinHook.h>

#include <vector>
#include <memory>
#include <cassert>

using namespace std;
using namespace AsmHacks;


#define LOG_FILE FOLDER "dll.log"

#define FRAME_INTERVAL ( 1000 / 60 )

#define WRITE_ASM_HACK(ASM_HACK)                                                \
    do {                                                                        \
        WindowsException err;                                                   \
        if ( ( err = ASM_HACK.write() ).code != 0 ) {                           \
            LOG ( "%s; %s failed", err, #ASM_HACK );                            \
            exit ( 0 );                                                         \
        }                                                                       \
    } while ( 0 )


static void deinitialize();


struct Main : public Socket::Owner, public Timer::Owner, public ControllerManager::Owner
{
    HANDLE pipe;
    SocketPtr ipcSocket;
    Timer timer;

    void acceptEvent ( Socket *serverSocket ) override
    {
        if ( serverSocket == ipcSocket.get() )
            ipcSocket = serverSocket->accept ( this );
    }

    void connectEvent ( Socket *socket ) override
    {
        LOG ( "Socket %08x connected", socket );
    }

    void disconnectEvent ( Socket *socket ) override
    {
        LOG ( "Socket %08x disconnected", socket );
    }

    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "Got %s from '%s'; socket=%08x", msg, address, socket );

        switch ( msg->getMsgType() )
        {
            case MsgType::ExitGame:
                EventManager::get().stop();
                break;

            default:
                break;
        }

        // if ( msg->getMsgType() == MsgType::SocketShareData )
        // {
        //     if ( msg->getAs<SocketShareData>().isTCP() )
        //         sharedSocket = TcpSocket::shared ( this, msg->getAs<SocketShareData>() );
        //     else
        //         sharedSocket = UdpSocket::shared ( this, msg->getAs<SocketShareData>() );

        //     MsgPtr msg ( new IpAddrPort ( sharedSocket->getRemoteAddress() ) );
        //     ipcSocket->send ( msg, address );
        // }

        // assert ( false );
    }

    void timerExpired ( Timer *timer ) override
    {
        assert ( timer == &this->timer );
    }

    Main() : pipe ( 0 ), ipcSocket ( TcpSocket::listen ( this, 0 ) ), timer ( this )
    {
        LOG ( "Connecting pipe" );

        pipe = CreateFile (
                   NAMED_PIPE,                              // name of the pipe
                   GENERIC_READ | GENERIC_WRITE,            // 2-way pipe
                   FILE_SHARE_READ | FILE_SHARE_WRITE,      // R/W sharing mode
                   0,                                       // default security
                   OPEN_EXISTING,                           // open existing pipe
                   FILE_ATTRIBUTE_NORMAL,                   // default attributes
                   0 );                                     // no template file

        if ( pipe == INVALID_HANDLE_VALUE )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "CreateFile failed" );
        }

        LOG ( "Pipe connected" );

        DWORD bytes;

        if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "WriteFile failed" );
        }

        if ( bytes != sizeof ( ipcSocket->address.port ) )
        {
            Exception err = toString ( "WriteFile wrote %d bytes, expected %d",
                                       bytes, sizeof ( ipcSocket->address.port ) );
            LOG_AND_THROW ( err, "" );
        }

        int processId = GetCurrentProcessId();

        if ( !WriteFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "WriteFile failed" );
        }

        if ( bytes != sizeof ( processId ) )
        {
            Exception err = toString ( "WriteFile wrote %d bytes, expected %d",
                                       bytes, sizeof ( ipcSocket->address.port ) );
            LOG_AND_THROW ( err, "" );
        }
    }

    ~Main()
    {
        if ( pipe )
            CloseHandle ( pipe );

        ipcSocket.reset();
    }
};

static enum State { UNINITIALIZED, POLLING, STOPPING, DEINITIALIZED } state = UNINITIALIZED;

static shared_ptr<Main> main;

static Mutex deinitMutex;

static uint64_t frameInterval = FRAME_INTERVAL;

extern "C" void callback()
{
    if ( state == DEINITIALIZED )
        return;

    try
    {
        do
        {
            if ( state == UNINITIALIZED )
            {
                // Joystick and timer must be initialized in the main thread
                TimerManager::get().initialize();
                ControllerManager::get().initialize ( main.get() );
                EventManager::get().startPolling();

                if ( detectWine() )
                {
                    // Temporary work around for Wine FPS limit issue
                    frameInterval = 5;
                }
                else
                {
                    // Asm hacks that must written after the game starts
                    WRITE_ASM_HACK ( disableFpsLimit );

                    // Hook DirectX
                    void *hwnd;
                    string err;
                    if ( ( hwnd = enumFindWindow ( CC_TITLE ) ) == 0 )
                        LOG ( "Couldn't find window '%s'", CC_TITLE );
                    else if ( ! ( err = InitDirectX ( hwnd ) ).empty() )
                        LOG ( "InitDirectX failed: %s", err );
                    else if ( ! ( err = HookDirectX() ).empty() )
                        LOG ( "HookDirectX failed: %s", err );
                }

                state = POLLING;
            }

            if ( state != POLLING )
                break;

            // Poll for events
            if ( !EventManager::get().poll ( frameInterval ) )
            {
                state = STOPPING;
                break;
            }
        }
        while ( 0 );
    }
    catch ( const WindowsException& err )
    {
        state = STOPPING;
    }
    catch ( ... )
    {
        LOG ( "Unknown exception!" );
        state = STOPPING;
    }

    if ( state == STOPPING )
    {
        LOG ( "Exiting" );
        ControllerManager::get().deinitialize(); // Joystick must be deinitialized on the same thread
        deinitialize();
        exit ( 0 );
    }
}

static const Asm hookCallback1 =
{
    MM_HOOK_CALL1_ADDR,
    {
        0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - MM_HOOK_CALL1_ADDR - 5 ), // call callback
        0xE9, INLINE_DWORD ( MM_HOOK_CALL2_ADDR - MM_HOOK_CALL1_ADDR - 10 ) // jmp MM_HOOK_CALL2_ADDR
    }
};

extern "C" BOOL APIENTRY DllMain ( HMODULE, DWORD reason, LPVOID )
{
    switch ( reason )
    {
        case DLL_PROCESS_ATTACH:
            Logger::get().initialize ( LOG_FILE );
            LOG ( "DLL_PROCESS_ATTACH" );

            try
            {
                SocketManager::get().initialize();

                main.reset ( new Main() );

                WRITE_ASM_HACK ( hookCallback1 );
                WRITE_ASM_HACK ( hookCallback2 );
                WRITE_ASM_HACK ( loopStartJump ); // Write the jump location last, due to dependencies on the above

                for ( void *const addr : disabledStageAddrs )
                {
                    Asm enableStage = { addr, INLINE_DWORD_FF };
                    WindowsException err;

                    if ( ( err = enableStage.write() ).code )
                    {
                        LOG ( "%s; enableStage { %08x } failed", err, addr );
                        exit ( 0 );
                    }
                }

                WRITE_ASM_HACK ( fixRyougiStageMusic1 );
                WRITE_ASM_HACK ( fixRyougiStageMusic2 );
            }
            catch ( const WindowsException& err )
            {
                exit ( 0 );
            }
            catch ( ... )
            {
                LOG ( "Unknown exception!" );
                exit ( 0 );
            }

            break;

        case DLL_PROCESS_DETACH:
            LOG ( "DLL_PROCESS_DETACH" );
            deinitialize();
            break;
    }

    return TRUE;
}

static void deinitialize()
{
    LOCK ( deinitMutex );

    if ( state == DEINITIALIZED )
        return;

    main.reset();

    EventManager::get().release();
    TimerManager::get().deinitialize();
    SocketManager::get().deinitialize();
    // Joystick must be deinitialized on the same thread it was initialized
    Logger::get().deinitialize();

    loopStartJump.revert();
    hookCallback2.revert();
    hookCallback1.revert();

    state = DEINITIALIZED;
}

static ID3DXFont *font = 0;

void PresentFrameBegin ( IDirect3DDevice9 *device )
{
    if ( !font )
    {
        D3DXCreateFont (
            device,
            24,                             // height
            0,                              // width
            FW_NORMAL,                      // weight
            1,                              // # of mipmap levels
            FALSE,                          // italic
            DEFAULT_CHARSET,                // charset
            OUT_DEFAULT_PRECIS,             // output precision
            ANTIALIASED_QUALITY,            // quality
            DEFAULT_PITCH | FF_DONTCARE,    // pitch and family
            "Courier New",                  // typeface name
            &font );
    }

    D3DVIEWPORT9 viewport;
    device->GetViewport ( &viewport );

    // This should be the only viewport with the same width as the main viewport
    if ( viewport.Width == * ( uint32_t * ) CC_SCREEN_WIDTH_ADDR )
    {
        const long centerX = ( long ) viewport.Width / 2;
        const long centerY = ( long ) viewport.Height / 2;

        RECT rect;
        rect.left    = centerX - 200;
        rect.right   = centerX + 200;
        rect.top     = 0;
        rect.bottom  = 20;

        font->DrawText (
            0,                              // Text as a ID3DXSprite object
            "Lorem ipsum dolor sit amet",   // Text as a C-string
            -1,                             // Number of letters, -1 for null-terminated
            &rect,                          // Text bounding RECT
            DT_CENTER,                      // Text formatting
            D3DCOLOR_XRGB ( 0, 255, 0 ) );  // Text color
    }
}

void PresentFrameEnd ( IDirect3DDevice9 *device )
{
}

void InvalidateDeviceObjects()
{
    if ( font )
    {
        font->OnLostDevice();
        font = 0;
    }
}
