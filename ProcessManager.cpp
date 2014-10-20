#include "ProcessManager.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "Utilities.h"
#include "Messages.h"
#include "Constants.h"
#include "AsmHacks.h"
#include "EventManager.h"

#include <windows.h>
#include <direct.h>

#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;


#define GAME_START_INTERVAL     ( 1000 )

#define GAME_START_ATTEMPTS     ( 10 )

#define PIPE_CONNECT_TIMEOUT    ( 5000 )


string ProcessManager::gameDir;

void ProcessManager::writeGameInput ( uint8_t player, uint16_t direction, uint16_t buttons )
{
    // LOG ( "player=%d; direction=%d; buttons=%04x", player, direction, buttons );

    if ( direction == 5 )
        direction = 0;

    ASSERT ( direction >= 0 );
    ASSERT ( direction <= 9 );

    char *const baseAddr = * ( char ** ) CC_PTR_TO_WRITE_INPUT_ADDR;

    switch ( player )
    {
        case 1:
            ( * ( uint16_t * ) ( baseAddr + CC_P1_OFFSET_DIRECTION ) ) = direction;
            ( * ( uint16_t * ) ( baseAddr + CC_P1_OFFSET_BUTTONS ) ) = buttons;
            break;

        case 2:
            ( * ( uint16_t * ) ( baseAddr + CC_P2_OFFSET_DIRECTION ) ) = direction;
            ( * ( uint16_t * ) ( baseAddr + CC_P2_OFFSET_BUTTONS ) ) = buttons;
            break;

        default:
            LOG_AND_THROW_STRING ( "Invalid player number!" );
            break;
    }
}

MsgPtr ProcessManager::getRngState() const
{
    RngState *rngState = new RngState();

    rngState->rngState0 = *CC_RNGSTATE0_ADDR;
    rngState->rngState1 = *CC_RNGSTATE1_ADDR;
    rngState->rngState2 = *CC_RNGSTATE2_ADDR;
    copy ( CC_RNGSTATE3_ADDR, CC_RNGSTATE3_ADDR + CC_RNGSTATE3_SIZE, rngState->rngState3.begin() );

    return MsgPtr ( rngState );
}

void ProcessManager::setRngState ( const RngState& rngState )
{
    *CC_RNGSTATE0_ADDR = rngState.rngState0;
    *CC_RNGSTATE1_ADDR = rngState.rngState1;
    *CC_RNGSTATE2_ADDR = rngState.rngState2;
    copy ( rngState.rngState3.begin(), rngState.rngState3.end(), CC_RNGSTATE3_ADDR );
}

void ProcessManager::acceptEvent ( Socket *serverSocket )
{
    ASSERT ( serverSocket == ipcSocket.get() );
    ASSERT ( serverSocket->isServer() == true );

    ipcSocket = serverSocket->accept ( this );

    ASSERT ( ipcSocket->address.addr == "127.0.0.1" );

    LOG ( "ipcSocket=%08x", ipcSocket.get() );

    ipcSocket->send ( new IpcConnected() );
}

void ProcessManager::connectEvent ( Socket *socket )
{
    ASSERT ( socket == ipcSocket.get() );
    ASSERT ( ipcSocket->address.addr == "127.0.0.1" );

    ipcSocket->send ( new IpcConnected() );
}

void ProcessManager::disconnectEvent ( Socket *socket )
{
    ASSERT ( socket == ipcSocket.get() );

    disconnectPipe();

    LOG ( "IPC disconnected" );

    if ( owner )
        owner->ipcDisconnectEvent();
}

void ProcessManager::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    ASSERT ( socket == ipcSocket.get() );
    ASSERT ( address.addr == "127.0.0.1" );

    if ( msg && msg->getMsgType() == MsgType::IpcConnected )
    {
        ASSERT ( connected == false );

        connected = true;
        gameStartTimer.reset();

        if ( owner )
            owner->ipcConnectEvent();
        return;
    }

    owner->ipcReadEvent ( msg );
}

void ProcessManager::timerExpired ( Timer *timer )
{
    ASSERT ( timer == gameStartTimer.get() );

    if ( gameStartCount >= GAME_START_ATTEMPTS && !ipcConnected() )
    {
        disconnectPipe();

        LOG ( "Failed to start game" );

        if ( owner )
            owner->ipcDisconnectEvent();
        return;
    }

    gameStartTimer->start ( GAME_START_INTERVAL );
    ++gameStartCount;

    LOG ( "Trying to start game (%d)", gameStartCount );

    void *hwnd = 0;
    if ( ! ( hwnd = enumFindWindow ( CC_STARTUP_TITLE_EN ) )
            && ! ( hwnd = enumFindWindow ( CC_STARTUP_TITLE_JP ) ) )
        return;

    if ( ! ( hwnd = FindWindowEx ( ( HWND ) hwnd, 0, 0, CC_STARTUP_BUTTON ) ) )
        return;

    if ( !PostMessage ( ( HWND ) hwnd, BM_CLICK, 0, 0 ) )
        return;
}

void ProcessManager::openGame()
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
        LOG_AND_THROW_ERROR ( err, "CreateNamedPipe failed" );
    }

    LOG ( "Running " MBAA_EXE );

    char buffer[4096];
    _getcwd ( buffer, sizeof ( buffer ) - 1 );

    string cwd = buffer;

    LOG ( "Working dir: %s", cwd );

    if ( !gameDir.empty() && ( gameDir.back() == '/' || gameDir.back() == '\\' ) )
        gameDir.resize ( gameDir.size() - 1 );

    LOG ( "Game dir: %s", gameDir );

    string command;

    if ( detectWine() )
    {
        command = "cd " + gameDir + " && " + cwd + "/" LAUNCHER " " MBAA_EXE " " + cwd + "/" + HOOK_DLL " &";

        replace ( command.begin(), command.end(), '\\', '/' );
    }
    else
    {
        command = "@start > nul 2>&1 \"\"";
        if ( !gameDir.empty() )
            command += " /d\"" + gameDir + "\"";
        command += " " + cwd + "\\" + LAUNCHER " ";
        if ( !gameDir.empty() )
            command += gameDir + "\\";
        command += MBAA_EXE " " + cwd + "\\" + HOOK_DLL;

        auto begin = command.begin();
        if ( !gameDir.empty() )
            begin = command.begin() + ( command.find ( "/d" ) + 2 );

        replace ( begin, command.end(), '/', '\\' );
    }

    LOG ( "Command: %s", command );

    system ( command.c_str() );

    LOG ( "Connecting pipe" );

    struct TimeoutThread : public Thread
    {
        void run() override
        {
            Sleep ( PIPE_CONNECT_TIMEOUT );

            HANDLE pipe = CreateFile ( NAMED_PIPE, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );

            if ( pipe == INVALID_HANDLE_VALUE )
                return;

            CloseHandle ( pipe );
        }
    };

    ThreadPtr thread ( new TimeoutThread() );
    thread->start();
    EventManager::get().addThread ( thread );

    if ( !ConnectNamedPipe ( pipe, 0 ) )
    {
        int error = GetLastError();

        if ( error != ERROR_PIPE_CONNECTED )
        {
            WindowsException err = GetLastError();
            LOG_AND_THROW_ERROR ( err, "ConnectNamedPipe failed" );
        }
    }

    LOG ( "Pipe connected" );

    DWORD bytes;
    IpAddrPort ipcHost ( "127.0.0.1", 0 );

    if ( !ReadFile ( pipe, &ipcHost.port, sizeof ( ipcHost.port ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW_ERROR ( err, "ReadFile failed" );
    }

    if ( bytes != sizeof ( ipcHost.port ) )
    {
        Exception err = toString ( "read %d bytes, expected %d", bytes, sizeof ( ipcHost.port ) );
        LOG_AND_THROW_ERROR ( err, "ReadFile failed" );
    }

    LOG ( "ipcHost='%s'", ipcHost );

    ipcSocket = TcpSocket::connect ( this, ipcHost );

    LOG ( "ipcSocket=%08x", ipcSocket.get() );

    if ( !ReadFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW_ERROR ( err, "ReadFile failed" );
    }

    if ( bytes != sizeof ( processId ) )
    {
        Exception err = toString ( "read %d bytes, expected %d", bytes, sizeof ( processId ) );
        LOG_AND_THROW_ERROR ( err, "ReadFile failed" );
    }

    LOG ( "processId=%08x", processId );

    gameStartTimer.reset ( new Timer ( this ) );
    gameStartTimer->start ( GAME_START_INTERVAL );
    gameStartCount = 0;
}

void ProcessManager::closeGame()
{
    disconnectPipe();

    LOG ( "Closing game" );

    // Find and close any lingering windows
    for ( const string& window : { CC_TITLE, CC_STARTUP_TITLE_EN, CC_STARTUP_TITLE_JP } )
    {
        void *hwnd;
        if ( ( hwnd = enumFindWindow ( window ) ) )
            PostMessage ( ( HWND ) hwnd, WM_CLOSE, 0, 0 );
    }
}

void ProcessManager::connectPipe()
{
    LOG ( "Listening on IPC socket" );

    ipcSocket = TcpSocket::listen ( this, 0 );

    LOG ( "Creating pipe" );

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
        LOG_AND_THROW_ERROR ( err, "CreateFile failed" );
    }

    LOG ( "Pipe created" );

    DWORD bytes;

    if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW_ERROR ( err, "WriteFile failed" );
    }

    if ( bytes != sizeof ( ipcSocket->address.port ) )
    {
        Exception err = toString ( "wrote %d bytes, expected %d", bytes, sizeof ( ipcSocket->address.port ) );
        LOG_AND_THROW_ERROR ( err, "WriteFile failed" );
    }

    processId = GetCurrentProcessId();

    if ( !WriteFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
    {
        WindowsException err = GetLastError();
        LOG_AND_THROW_ERROR ( err, "WriteFile failed" );
    }

    if ( bytes != sizeof ( processId ) )
    {
        Exception err = toString ( "wrote %d bytes, expected %d", bytes, sizeof ( ipcSocket->address.port ) );
        LOG_AND_THROW_ERROR ( err, "WriteFile failed" );
    }
}

void ProcessManager::disconnectPipe()
{
    gameStartTimer.reset();
    ipcSocket.reset();

    if ( pipe )
    {
        CloseHandle ( ( HANDLE ) pipe );
        pipe = 0;
    }

    connected = false;
}

ProcessManager::ProcessManager ( Owner *owner ) : owner ( owner ) {}

ProcessManager::~ProcessManager()
{
    disconnectPipe();
}

string ProcessManager::fetchGameUserName()
{
    if ( !gameDir.empty() && ( gameDir.back() == '/' || gameDir.back() == '\\' ) )
        gameDir.resize ( gameDir.size() - 1 );

    string path;
    if ( !gameDir.empty() )
        path = gameDir + ( detectWine() ? "/" : "\\" );
    path += CC_NETWORK_CONFIG_FILE;

    if ( detectWine() )
        replace ( path.begin(), path.end(), '\\', '/' );
    else
        replace ( path.begin(), path.end(), '/', '\\' );

    LOG ( "Opening: %s", path );

    string line, buffer;
    ifstream fin ( path );

    while ( getline ( fin, line ) )
    {
        buffer.clear();

        if ( line.substr ( 0, sizeof ( CC_NETWORK_USERNAME_KEY ) - 1 ) == CC_NETWORK_USERNAME_KEY )
        {
            // Find opening quote
            size_t pos = line.find ( '"' );
            if ( pos == string::npos )
                break;

            buffer = line.substr ( pos + 1 );

            // Find closing quote
            pos = buffer.rfind ( '"' );
            if ( pos == string::npos )
                break;

            buffer.erase ( pos );
            break;
        }
    }

    fin.close();
    return buffer;
}
