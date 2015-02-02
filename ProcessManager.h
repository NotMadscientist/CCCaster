#pragma once

#include "Socket.h"
#include "Timer.h"
#include "Protocol.h"
#include "Messages.h"

#include <array>


#define COMBINE_INPUT(DIRECTION, BUTTONS)   uint16_t ( ( DIRECTION ) | ( ( BUTTONS ) << 4 ) )

#define INLINE_INPUT(INPUT)                 uint16_t ( ( INPUT ) & 0x000Fu ), uint16_t ( ( ( INPUT ) & 0xFFF0u ) >> 4 )


struct IpcConnected : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( IpcConnected ) };


class ProcessManager : public Socket::Owner, public Timer::Owner
{
public:

    struct Owner
    {
        // IPC connected event
        virtual void ipcConnectEvent() = 0;

        // IPC disconnected event
        virtual void ipcDisconnectEvent() = 0;

        // IPC read event
        virtual void ipcReadEvent ( const MsgPtr& msg ) = 0;
    };

    Owner *owner = 0;

private:

    // Named pipe
    void *pipe = 0;

    // Process ID
    int processId = 0;

    // IPC socket
    SocketPtr ipcSocket;

    // Game start timer
    TimerPtr gameStartTimer;

    // Number of attempts to start the game
    int gameStartCount = 0;

    // IPC connected flag
    bool connected = false;

    // IPC socket callbacks
    void acceptEvent ( Socket *socket ) override;
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

    // IPC connect timer callback
    void timerExpired ( Timer *timer ) override;

public:

    // Game directory, empty means current working directory
    static std::string gameDir;


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
    void openGame ( const std::string& appDir, bool highPriority = false );
    void closeGame();

    // Connect / disconnect the IPC pipe and socket from the DLL side
    void connectPipe();
    void disconnectPipe();

    // Indicates if the IPC pipe and socket are connected
    bool isConnected() const { return ( pipe && ipcSocket && ipcSocket->isClient() && connected ); }

    // Send a message over the IPC socket
    bool ipcSend ( Serializable& msg ) { return ipcSend ( MsgPtr ( &msg, ignoreMsgPtr ) ); }
    bool ipcSend ( Serializable *msg ) { return ipcSend ( MsgPtr ( msg ) ); }
    bool ipcSend ( const MsgPtr& msg ) { if ( !isConnected() ) return false; else return ipcSocket->send ( msg ); }

    // Get the process ID of the game
    int getProcessId() const { return processId; }

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
};
