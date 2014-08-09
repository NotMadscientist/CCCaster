#pragma once

#include "Socket.h"
#include "Timer.h"
#include "Protocol.h"


#define COMBINE_INPUT(DIRECTION, BUTTONS)   ( ( DIRECTION ) | ( ( BUTTONS ) << 4 ) )

#define INLINE_INPUT(INPUT)                 ( ( INPUT ) & 0x000F ), ( ( ( INPUT ) & 0xFFF0 ) >> 4 )


struct IpcConnected : public SerializableSequence
{
    EMPTY_MESSAGE_BOILERPLATE ( IpcConnected )
};


class ProcessManager : public Socket::Owner, public Timer::Owner
{
public:

    struct Owner
    {
        // IPC connected event
        virtual void ipcConnectEvent() {}

        // IPC disconnected event
        virtual void ipcDisconnectEvent() {}

        // IPC read event
        virtual void ipcReadEvent ( const MsgPtr& msg ) {}
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

    // IPC connect timer
    TimerPtr ipcConnectTimer;

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

    // Basic constructor / destructor
    ProcessManager ( Owner *owner );
    ~ProcessManager();

    // Open / close the game from the EXE side
    void openGame();
    void closeGame();

    // Connect / disconnect the IPC pipe and socket from the DLL side
    void connectPipe();
    void disconnectPipe();

    // Indicates if the IPC pipe and socket are connected
    bool ipcConnected() const { return ( pipe && ipcSocket && ipcSocket->isClient() && connected ); }

    // Send a message over the IPC socket
    bool ipcSend ( Serializable *msg ) { return ipcSend ( MsgPtr ( msg ) ); }
    bool ipcSend ( const MsgPtr& msg ) { if ( !ipcSocket ) return false; else return ipcSocket->send ( msg ); }

    // Get the process ID of the game
    int getProcessId() const { return processId; }

    // Write game input
    void writeGameInput ( uint8_t player, uint16_t direction, uint16_t buttons );

    void writeGameInput ( uint8_t player, uint16_t input )
    {
        writeGameInput ( player, INLINE_INPUT ( input ) );
    }

    // Get the game RNG state
    MsgPtr getRngState() const;
};
