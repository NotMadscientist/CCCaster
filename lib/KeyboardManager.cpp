#include "KeyboardManager.h"
#include "Logger.h"
#include "Thread.h"
#include "UdpSocket.h"

#include <windows.h>

using namespace std;


// Window to match for keyboard events, 0 to match all, NOT safe to modify when hooked!
static const void *keyboardWindow = 0;

// VK codes to match for keyboard events, empty to match all, NOT safe to modify when hooked!
static unordered_set<int> keyboardKeys;

// Keyboard hooking options
static uint8_t hookOptions = 0;

// Keyboard hook and message loop thread
static ThreadPtr keyboardThread;

// Socket to send KeyboardEvent messages
static SocketPtr sendSocket;


// The variables above are statically declared in this file because of the following callback function.
// This function can't be a friend of KeyboardManager, because its function signature requires Windows
// types which shouldn't be exposed in the public header. Thus this function can't easily access the
// private members of KeyboardManager, so instead the variables needed are just static in this file.
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

                // LOG ( "code=%d; wParam=%d; lParam=%d; vkCode=%d", code, wParam, lParam, vkCode );

                // Ignore key if socket is unconnected
                if ( !sendSocket || !sendSocket->isConnected() )
                    break;

                // Ignore key if the window does not match
                if ( keyboardWindow && ( GetForegroundWindow() != keyboardWindow ) )
                    break;

                // Ignore key if the vkCode is not matched
                if ( !keyboardKeys.empty() && ( keyboardKeys.find ( vkCode ) == keyboardKeys.end() ) )
                    break;

                // Send KeyboardEvent message and return 1 to eat the keyboard event
                sendSocket->send ( new KeyboardEvent ( vkCode, ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN ) ) );
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
        // and it's not safe to call virtual functions in the ctor/dtor,
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


void KeyboardManager::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    ASSERT ( socket == recvSocket.get() );

    if ( !msg.get() || msg->getMsgType() != MsgType::KeyboardEvent )
    {
        LOG ( "Unexpected '%s'", msg );
        return;
    }

    LOG ( "vkCode=%d; isDown=%d", msg->getAs<KeyboardEvent>().vkCode, msg->getAs<KeyboardEvent>().isDown );

    if ( owner )
        owner->keyboardEvent ( msg->getAs<KeyboardEvent>().vkCode, msg->getAs<KeyboardEvent>().isDown );
}

void KeyboardManager::hook ( Owner *owner, const void *window, const std::unordered_set<int>& keys, uint8_t options )
{
    LOG ( "Hooking keyboard manager" );

    this->owner = owner;
    keyboardWindow = window;
    keyboardKeys = keys;
    hookOptions = options;

    recvSocket = UdpSocket::bind ( this, 0 );
    sendSocket = UdpSocket::bind ( 0, { "127.0.0.1", recvSocket->address.port } );

    keyboardThread.reset ( new KeyboardThread() );
    keyboardThread->start();
}

void KeyboardManager::unhook()
{
    LOG ( "Unhooking keyboard manager" );

    keyboardThread.reset();
    sendSocket.reset();
    recvSocket.reset();
}

KeyboardManager& KeyboardManager::get()
{
    static KeyboardManager km;
    return km;
}
