#pragma once

#include "Socket.hpp"
#include "Timer.hpp"
#include "Protocol.hpp"
#include "Messages.hpp"

#include <array>


#define COMBINE_INPUT(DIRECTION, BUTTONS)   uint16_t ( ( DIRECTION ) | ( ( BUTTONS ) << 4 ) )

#define INLINE_INPUT(INPUT)                 uint16_t ( ( INPUT ) & 0x000Fu ), uint16_t ( ( ( INPUT ) & 0xFFF0u ) >> 4 )


struct IpcConnected : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( IpcConnected ) };


class ProcessManager
    : private Socket::Owner
    , private Timer::Owner
{
public:

    struct Owner
    {
        // IPC connected event
        virtual void ipcConnected() = 0;

        // IPC disconnected event
        virtual void ipcDisconnected() = 0;

        // IPC read event
        virtual void ipcRead ( const MsgPtr& msg ) = 0;
    };

    Owner *owner = 0;

    // Game directory, empty means current working directory
    static std::string gameDir;

    // Application directory, empty means current working directory
    static std::string appDir;


    // Get / set windowed mode
    static bool getIsWindowed();
    static void setIsWindowed ( bool enabled );

    // Get the user name from the game config
    static std::string fetchGameUserName();

    // Get the keyboard config from the game binary
    static std::array<char, 10> fetchKeyboardConfig();

    // Find the first window handle with the given title (NOT thread safe)
    static void *findWindow ( const std::string& title, bool exact = true );

    // True if we're running on Wine, this caches the result of the first call (NOT thread safe)
    static bool isWine();

    // Basic constructor / destructor
    ProcessManager ( Owner *owner );
    ~ProcessManager();

    // Open / close the game from the EXE side
    void openGame ( bool highPriority = false );
    void closeGame();

    // Connect / disconnect the IPC pipe and socket from the DLL side
    void connectPipe();
    void disconnectPipe();

    // Indicates if the IPC pipe and socket are connected
    bool isConnected() const;

    // Send a message over the IPC socket
    bool ipcSend ( Serializable& msg );
    bool ipcSend ( Serializable *msg );
    bool ipcSend ( const MsgPtr& msg );

    // Get the process ID of the game
    int getProcessId() const { return _processId; }

    // Write game input
    void writeGameInput ( uint8_t player, uint16_t direction, uint16_t buttons );

    void writeGameInput ( uint8_t player, uint16_t input )
    {
        writeGameInput ( player, INLINE_INPUT ( input ) );
    }

    void clearInputs()
    {
        writeGameInput ( 1, 0, 0 );
        writeGameInput ( 2, 0, 0 );
    }

    // Get / set the game RngState
    MsgPtr getRngState ( uint32_t index ) const;
    void setRngState ( const RngState& rngState );

private:

    // Named pipe
    void *_pipe = 0;

    // Process ID
    int _processId = 0;

    // IPC socket
    SocketPtr _ipcSocket;

    // Game start timer
    TimerPtr _gameStartTimer;

    // Number of attempts to start the game
    int _gameStartCount = 0;

    // IPC connected flag
    bool _connected = false;

    // IPC socket callbacks
    void socketAccepted ( Socket *socket ) override;
    void socketConnected ( Socket *socket ) override;
    void socketDisconnected ( Socket *socket ) override;
    void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

    // IPC connect timer callback
    void timerExpired ( Timer *timer ) override;
};
