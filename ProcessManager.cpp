#include "ProcessManager.h"
#include "TcpSocket.h"
#include "Messages.h"
#include "Constants.h"
#include "AsmHacks.h"
#include "EventManager.h"
#include "Exceptions.h"
#include "ErrorStringsExt.h"

#include <windows.h>
#include <direct.h>

#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;


#define GAME_START_INTERVAL     ( 1000 )

#define GAME_START_ATTEMPTS     ( 20 )

#define PIPE_CONNECT_TIMEOUT    ( 5000 )


string ProcessManager::gameDir;

void ProcessManager::writeGameInput ( uint8_t player, uint16_t direction, uint16_t buttons )
{
    if ( direction == 5 || direction < 0 || direction > 9 )
        direction = 0;

    ASSERT ( direction >= 0 );
    ASSERT ( direction <= 9 );

    // LOG ( "player=%d; direction=%d; buttons=%04x", player, direction, buttons );

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
            ASSERT_IMPOSSIBLE;
            break;
    }
}

MsgPtr ProcessManager::getRngState ( uint32_t index ) const
{
    RngState *rngState = new RngState ( index );

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

    if ( gameStartCount >= GAME_START_ATTEMPTS && !isConnected() )
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
    if ( ! ( hwnd = findWindow ( CC_STARTUP_TITLE_EN ) )
            && ! ( hwnd = findWindow ( CC_STARTUP_TITLE_JP ) ) )
        return;

    if ( ! ( hwnd = FindWindowEx ( ( HWND ) hwnd, 0, 0, CC_STARTUP_BUTTON ) ) )
        return;

    if ( !PostMessage ( ( HWND ) hwnd, BM_CLICK, 0, 0 ) )
        return;
}

void ProcessManager::openGame ( const string& appDir, bool highPriority )
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
        THROW_WIN_EXCEPTION ( GetLastError(), "CreateNamedPipe failed", ERROR_PIPE_OPEN );

    LOG ( "appDir='%s'", appDir );
    LOG ( "gameDir='%s'", gameDir );

    string command;

    if ( gameDir.empty() )
        command = "cd \"" + appDir + "\" && ";
    else
        command = "cd \"" + gameDir + "\" && ";

    command += "\"" + appDir + LAUNCHER "\" \"" + MBAA_EXE "\" \"" + appDir + HOOK_DLL "\"";

    if ( highPriority )
        command += " --high";

    LOG ( "Running: %s", command );

    int returnCode = system ( ( "\"" + command + "\"" ).c_str() );

    LOG ( "returnCode=%d", returnCode );

    if ( returnCode < 0 )
        THROW_EXCEPTION ( "returnCode=%d", ERROR_PIPE_START, returnCode );

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
            THROW_WIN_EXCEPTION ( GetLastError(), "ConnectNamedPipe failed", ERROR_PIPE_START );
    }

    LOG ( "Pipe connected" );

    DWORD bytes;
    IpAddrPort ipcHost ( "127.0.0.1", 0 );

    if ( !ReadFile ( pipe, &ipcHost.port, sizeof ( ipcHost.port ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "ReadFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( ipcHost.port ) )
        THROW_EXCEPTION ( "read %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( ipcHost.port ) );

    LOG ( "ipcHost='%s'", ipcHost );

    ipcSocket = TcpSocket::connect ( this, ipcHost );

    LOG ( "ipcSocket=%08x", ipcSocket.get() );

    if ( !ReadFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "ReadFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( processId ) )
        THROW_EXCEPTION ( "read %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( processId ) );

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
        if ( ( hwnd = findWindow ( window ) ) )
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
        THROW_WIN_EXCEPTION ( GetLastError(), "CreateFile failed", ERROR_PIPE_START );

    LOG ( "Pipe created" );

    DWORD bytes;

    if ( !WriteFile ( pipe, & ( ipcSocket->address.port ), sizeof ( ipcSocket->address.port ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "WriteFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( ipcSocket->address.port ) )
        THROW_EXCEPTION ( "wrote %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( ipcSocket->address.port ) );

    processId = GetCurrentProcessId();

    if ( !WriteFile ( pipe, &processId, sizeof ( processId ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "WriteFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( processId ) )
        THROW_EXCEPTION ( "wrote %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( processId ) );
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
    const string file = gameDir + CC_NETWORK_CONFIG_FILE;

    LOG ( "Reading: %s", file );

    string line, buffer;
    ifstream fin ( file );

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

array<char, 10> ProcessManager::fetchKeyboardConfig()
{
    const string file = gameDir + MBAA_EXE;

    LOG ( "Reading: %s", file );

    array<char, 10> config;
    ifstream fin ( file, ios::binary );

    fin.seekg ( CC_KEYBOARD_CONFIG_OFFSET );
    fin.read ( &config[0], config.size() );

    return config;
}

void *ProcessManager::findWindow ( const string& title )
{
    static string tmpTitle;
    static HWND tmpHwnd;

    struct _
    {
        static BOOL CALLBACK enumWindowsProc ( HWND hwnd, LPARAM lParam )
        {
            if ( hwnd == 0 )
                return true;

            char buffer[4096];
            GetWindowText ( hwnd, buffer, sizeof ( buffer ) );

            if ( tmpTitle == trimmed ( buffer ) )
                tmpHwnd = hwnd;
            return true;
        }
    };

    tmpTitle = title;
    tmpHwnd = 0;
    EnumWindows ( _::enumWindowsProc, 0 );
    return tmpHwnd;
}


bool ProcessManager::isWine()
{
    static char isWine = -1; // -1 means uninitialized

    if ( isWine >= 0 )
        return isWine;

    HMODULE hntdll = GetModuleHandle ( "ntdll.dll" );

    if ( !hntdll )
    {
        isWine = 0;
        return isWine;
    }

    isWine = ( GetProcAddress ( hntdll, "wine_get_version" ) ? 1 : 0 );
    return isWine;
}
