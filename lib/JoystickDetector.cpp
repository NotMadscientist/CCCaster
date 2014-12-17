#include "JoystickDetector.h"
#include "ControllerManager.h"
#include "Thread.h"
#include "UdpSocket.h"
#include "Exceptions.h"
#include "ErrorStrings.h"

#define INITGUID
#include <windows.h>
#include <oleauto.h>
#include <dbt.h>

using namespace std;


#define JOYSTICK_THREAD_WAIT ( 300 )

DEFINE_GUID ( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 );


static SocketPtr sendSocket, recvSocket;

static LRESULT CALLBACK joystickCallback ( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
        case WM_DEVICECHANGE:
            switch ( wParam )
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
                    if ( ( ( DEV_BROADCAST_HDR * ) lParam )->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE )
                        break;

                    if ( !sendSocket || !sendSocket->isConnected() )
                        break;

                    sendSocket->send ( new JoysticksChanged() );
                    break;
            }
            return 0;

        default:
            break;
    }

    return DefWindowProc ( hwnd, message, wParam, lParam );
}


class JoystickThread : public Thread
{
    Mutex mutex;
    bool running = false;

public:

    ~JoystickThread()
    {
        // Since ~Thread calls Thread::join(), which gets overridden,
        // and it's not safe to call virtual functions in the ctor / dtor,
        // we must override the dtor and call JoystickThread::join().
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
        HRESULT comInitRet = CoInitializeEx ( 0, COINIT_APARTMENTTHREADED );
        if ( FAILED ( comInitRet ) )
            comInitRet = CoInitializeEx ( 0, COINIT_MULTITHREADED );
        if ( FAILED ( comInitRet ) )
            THROW_EXCEPTION ( "CoInitializeEx failed: 0x%08x", ERROR_CONTROLLER_INIT, comInitRet );

        WNDCLASSEX windowClass;
        memset ( &windowClass, 0, sizeof ( windowClass ) );
        windowClass.hInstance = GetModuleHandle ( 0 );
        windowClass.lpszClassName = "JoystickThread";
        windowClass.lpfnWndProc = joystickCallback;
        windowClass.cbSize = sizeof ( windowClass );

        if ( !RegisterClassEx ( &windowClass ) )
            THROW_WIN_EXCEPTION ( GetLastError(), "RegisterClassEx failed", ERROR_CONTROLLER_INIT );

        HWND windowHandle = CreateWindowEx ( 0,  "JoystickThread", 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0 );
        if ( ! windowHandle )
            THROW_WIN_EXCEPTION ( GetLastError(), "CreateWindowEx failed", ERROR_CONTROLLER_INIT );

        DEV_BROADCAST_DEVICEINTERFACE dbh;
        memset ( &dbh, 0, sizeof ( dbh ) );
        dbh.dbcc_size = sizeof ( dbh );
        dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

        HDEVNOTIFY notifyHandle = RegisterDeviceNotification ( windowHandle, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE );
        if ( ! notifyHandle )
            THROW_WIN_EXCEPTION ( GetLastError(), "CreateWindowEx failed", ERROR_CONTROLLER_INIT );

        MSG message;
        BOOL ret;
        for ( ;; )
        {
            {
                LOCK ( mutex );

                if ( !running )
                    break;
            }

            Sleep ( JOYSTICK_THREAD_WAIT );

            if ( ! PeekMessage ( &message, windowHandle, 0, 0, PM_NOREMOVE ) )
                continue;

            if ( ( ret = GetMessage ( &message, windowHandle, 0, 0 ) ) != 0 )
            {
                if ( ret == -1 )
                {
                    THROW_WIN_EXCEPTION ( GetLastError(), "GetMessage failed", ERROR_CONTROLLER_CHECK );
                }
                else
                {
                    TranslateMessage ( &message );
                    DispatchMessage ( &message );
                }
            }
        }

        if ( notifyHandle )
            UnregisterDeviceNotification ( notifyHandle );

        if ( windowHandle )
            DestroyWindow ( windowHandle );

        UnregisterClass ( windowClass.lpszClassName, windowClass.hInstance );

        if ( SUCCEEDED ( comInitRet ) )
            CoUninitialize();
    }
};

static JoystickThread joystickThread;


void JoystickDetector::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    ASSERT ( socket == recvSocket.get() );

    if ( !msg.get() || msg->getMsgType() != MsgType::JoysticksChanged )
    {
        LOG ( "Unexpected '%s'", msg );
        return;
    }

    ControllerManager::get().refreshJoysticks();
}

void JoystickDetector::start()
{
    if ( recvSocket || sendSocket )
        return;

    LOG ( "Starting joystick detection" );

    recvSocket = UdpSocket::bind ( this, 0 );
    sendSocket = UdpSocket::bind ( 0, { "127.0.0.1", recvSocket->address.port } );

    joystickThread.start();

    LOG ( "Started detecting joysticks" );
}

void JoystickDetector::stop()
{
    LOG ( "Stopping joystick detection" );

    joystickThread.join();

    sendSocket.reset();
    recvSocket.reset();

    LOG ( "Stopped detecting joysticks" );
}

JoystickDetector& JoystickDetector::get()
{
    static JoystickDetector instance;
    return instance;
}

