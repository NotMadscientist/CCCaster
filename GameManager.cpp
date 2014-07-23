#include "GameManager.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Messages.h"
#include "Constants.h"

#include <windows.h>

#include <cassert>

using namespace std;


void GameManager::connectEvent ( Socket *socket )
{
    assert ( socket == ipcSocket.get() );

    if ( owner )
        owner->gameOpened();
}

void GameManager::disconnectEvent ( Socket *socket )
{
    assert ( socket == ipcSocket.get() );

    ipcSocket.reset();

    if ( owner )
        owner->gameClosed();
}

void GameManager::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
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
    {
        ipcSocket->send ( new ExitGame() );
        ipcSocket.reset();
    }

    if ( pipe )
    {
        CloseHandle ( ( HANDLE ) pipe );
        pipe = 0;
    }

    // Find and close any lingering windows
    void *hwnd = 0;
    for ( const string& window : { CC_TITLE, CC_STARTUP_TITLE_EN, CC_STARTUP_TITLE_JP } )
    {
        if ( ( hwnd = enumFindWindow ( window ) ) )
            PostMessage ( ( HWND ) hwnd, WM_CLOSE, 0, 0 );
    }
}

