#include "GameManager.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Messages.h"
#include "Constants.h"

#include <windows.h>

#include <cassert>

using namespace std;


void GameManager::acceptEvent ( Socket *serverSocket )
{
    if ( serverSocket == ipcSocket.get() )
    {
        ipcSocket = serverSocket->accept ( this );
        assert ( ipcSocket->address.addr == "127.0.0.1" );

        if ( owner )
            owner->ipcConnectEvent();
    }
}

void GameManager::connectEvent ( Socket *socket )
{
    assert ( socket == ipcSocket.get() );

    if ( owner )
        owner->ipcConnectEvent();
}

void GameManager::disconnectEvent ( Socket *socket )
{
    assert ( socket == ipcSocket.get() );

    ipcSocket.reset();

    if ( owner )
        owner->ipcDisconnectEvent();
}

void GameManager::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    switch ( msg->getMsgType() )
    {
        case MsgType::ExitGame:
            if ( owner )
                owner->ipcDisconnectEvent();
            break;

        default:
            break;
    }
}

void GameManager::openGame()
{
    LOG ( "Opening pipe" );

    pipe = CreateNamedPipe (
               NAMED_PIPE,                                          // name of the pipe
               PIPE_ACCESS_DUPLEX,                                  // 2-way pipe
               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,     // byte stream + blocking
               1,                                                   // only allow 1 instance of this pipe
               1024,                                                // outbound buffer size
               1024,                                                // inbound buffer size
               0,                                                   // use default wait time
               0 );                                                 // use default security attributes

    if ( pipe == INVALID_HANDLE_VALUE )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW ( err, "CreateNamedPipe failed" );
    }

    LOG ( "Starting " MBAA_EXE );

    if ( detectWine() )
        system ( "./" LAUNCHER " " MBAA_EXE " " HOOK_DLL " &" );
    else
        system ( "start \"\" " LAUNCHER " " MBAA_EXE " " HOOK_DLL );

    LOG ( "Connecting pipe" );

    if ( !ConnectNamedPipe ( pipe, 0 ) )
    {
        int error = GetLastError();

        if ( error != ERROR_PIPE_CONNECTED )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW ( err, "ConnectNamedPipe failed" );
        }
    }

    LOG ( "Pipe connected" );

    DWORD bytes;
    IpAddrPort ipcHost ( "127.0.0.1", 0 );

    if ( !ReadFile ( pipe, &ipcHost.port, sizeof ( ipcHost.port ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW ( err, "ReadFile failed" );
    }

    if ( bytes != sizeof ( ipcHost.port ) )
    {
        Exception err = toString ( "ReadFile read %d bytes, expected %d", bytes, sizeof ( ipcHost.port ) );
        LOG_AND_THROW ( err, "" );
    }

    LOG ( "ipcHost='%s'", ipcHost );

    ipcSocket = TcpSocket::connect ( this, ipcHost );

    if ( !ReadFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW ( err, "ReadFile failed" );
    }

    if ( bytes != sizeof ( processId ) )
    {
        Exception err = toString ( "ReadFile read %d bytes, expected %d", bytes, sizeof ( processId ) );
        LOG_AND_THROW ( err, "" );
    }

    LOG ( "processId=%08x", processId );
}

void GameManager::closeGame()
{
    if ( ipcSocket )
        ipcSocket->send ( new ExitGame() );

    disconnectPipe();

    // Find and close any lingering windows
    for ( const string& window : { CC_TITLE, CC_STARTUP_TITLE_EN, CC_STARTUP_TITLE_JP } )
    {
        void *hwnd;
        if ( ( hwnd = enumFindWindow ( window ) ) )
            PostMessage ( ( HWND ) hwnd, WM_CLOSE, 0, 0 );
    }
}

void GameManager::connectPipe()
{
    LOG ( "Listening on IPC socket" );

    ipcSocket = TcpSocket::listen ( this, 0 );

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

    processId = GetCurrentProcessId();

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

void GameManager::disconnectPipe()
{
    ipcSocket.reset();

    if ( pipe )
    {
        CloseHandle ( ( HANDLE ) pipe );
        pipe = 0;
    }
}
