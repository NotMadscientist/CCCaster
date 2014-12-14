#include "KeyboardManager.h"
#include "Thread.h"
#include "UdpSocket.h"
#include "Exceptions.h"

#include <windows.h>

using namespace std;


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
                const KeyboardManager& km = KeyboardManager::get();

                // Ignore key if socket is unconnected
                if ( !km.sendSocket || !km.sendSocket->isConnected() )
                    break;

                // Ignore key if the window does not match
                if ( km.keyboardWindow && ( GetForegroundWindow() != km.keyboardWindow ) )
                    break;

                const uint32_t vkCode = ( uint32_t ) ( ( ( PKBDLLHOOKSTRUCT ) lParam )->vkCode );

                // Ignore key if the VK code is not matched
                if ( !km.matchedKeys.empty() && ( km.matchedKeys.find ( vkCode ) == km.matchedKeys.end() ) )
                    break;

                // Ignore key if it is explicitly ignored
                if ( km.ignoredKeys.find ( vkCode ) != km.ignoredKeys.end() )
                    break;

                const uint32_t scanCode = ( uint32_t ) ( ( ( PKBDLLHOOKSTRUCT ) lParam )->scanCode );
                const bool isExtended = ( ( ( PKBDLLHOOKSTRUCT ) lParam )->flags & LLKHF_EXTENDED );
                const bool isDown = ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN );

                // Send KeyboardEvent message and return 1 to eat the keyboard event
                km.sendSocket->send ( new KeyboardEvent ( vkCode, scanCode, isExtended, isDown ) );
                return 1;
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
        // and it's not safe to call virtual functions in the ctor / dtor,
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
            LOG ( "SetWindowsHookEx failed: %s", WinException::getLastError() );
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


static ThreadPtr keyboardThread;


void KeyboardManager::hookImpl()
{
    if ( keyboardThread )
        return;

    keyboardThread.reset ( new KeyboardThread() );
    keyboardThread->start();
}

void KeyboardManager::unhookImpl()
{
    keyboardThread.reset();
}
