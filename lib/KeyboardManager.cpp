#include "KeyboardManager.h"
#include "Logger.h"
#include "Thread.h"
#include "TcpSocket.h"
#include "UdpSocket.h"

#include <windows.h>

using namespace std;


static ThreadPtr keyboardThread;

static SocketPtr keyboardSocket;

static LRESULT CALLBACK keyboardCallback ( int code, WPARAM wParam, LPARAM lParam )
{
    if ( code == HC_ACTION )
    {
        switch ( wParam )
        {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                const int vkCode = ( int ) ( ( ( PKBDLLHOOKSTRUCT ) lParam )->vkCode );

                LOG ( "code=%d; wParam=%d; lParam=%d; vkCode=%d", code, wParam, lParam, vkCode );

                if ( keyboardSocket && keyboardSocket->isConnected()
                        && ( GetForegroundWindow() == KeyboardManager::get().window )
                        && ( KeyboardManager::get().keys.find ( vkCode ) != KeyboardManager::get().keys.end() ) )
                {
                    keyboardSocket->send ( new KeyboardEvent ( vkCode,
                                           ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN ) ) );

                    // Return 1 to eat key event
                    return 1;
                }

                break;
            }

            default:
                break;
        }
    }

    return CallNextHookEx ( 0, code, wParam, lParam );
}

class KeyboardThread : public Thread
{
    Mutex mutex;
    bool running = false;

public:

    ~KeyboardThread()
    {
        // Since ~Thread calls Thread::join(), which gets overridden,
        // and its not safe to call virtual functions in the ctor/dtor,
        // we must override the dtor and call KeyboardManager::join().
        join();
    }

    void start() override
    {
        {
            LOCK ( mutex );

            if ( running )
                return;

            running = true;
        }

        Thread::start();
    }

    void join() override
    {
        {
            LOCK ( mutex );

            if ( !running )
                return;

            running = false;
        }

        Thread::join();
    }

    void run() override
    {
        // The keyboard hook needs a dedicated event loop (in the same thread?)
        HHOOK keyboardHook = SetWindowsHookEx ( WH_KEYBOARD_LL, keyboardCallback, GetModuleHandle ( 0 ), 0 );

        if ( !keyboardHook )
        {
            LOG ( "SetWindowsHookEx failed: %s", WindowsException ( GetLastError() ) );
            return;
        }

        LOG ( "Keyboard hooked" );

        MSG msg;

        for ( ;; )
        {
            { LOCK ( mutex ); if ( !running ) break; }

            if ( PeekMessage ( &msg, 0, 0, 0, PM_REMOVE ) )
            {
                TranslateMessage ( &msg );
                DispatchMessage ( &msg );
            }

            if ( msg.message == WM_QUIT )
                break;

            Sleep ( 1 );
        }

        UnhookWindowsHookEx ( keyboardHook );

        LOG ( "Keyboard unhooked" );
    }
};

void KeyboardManager::hook ( const SocketPtr& eventSocket )
{
    LOG ( "Hooking keyboard manager" );

    ASSERT ( eventSocket.get() != 0 );

    if ( eventSocket->isTCP() )
        keyboardSocket = TcpSocket::connect ( 0, { "127.0.0.1", eventSocket->address.port } );
    else
        keyboardSocket = UdpSocket::bind ( 0, { "127.0.0.1", eventSocket->address.port } );

    keyboardThread.reset ( new KeyboardThread() );
    keyboardThread->start();
}

void KeyboardManager::unhook()
{
    LOG ( "Unhooking keyboard manager" );

    keyboardThread.reset();
    keyboardSocket.reset();
}

KeyboardManager& KeyboardManager::get()
{
    static KeyboardManager kt;
    return kt;
}
