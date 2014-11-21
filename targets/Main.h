#pragma once

#include "EventManager.h"
#include "ProcessManager.h"
#include "SocketManager.h"
#include "Socket.h"
#include "ControllerManager.h"
#include "Controller.h"
#include "TimerManager.h"
#include "Timer.h"
#include "KeyboardManager.h"
#include "IpAddrPort.h"
#include "Messages.h"
#include "Enum.h"

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>


// Log file that contains all the data needed to keep games in sync
#define SYNC_LOG_FILE FOLDER "sync.log"

// Default pending socket timeout
#define DEFAULT_PENDING_TIMEOUT ( 10000 )

// Set of command line options
ENUM ( Options,
       // Regular options
       Help, GameDir, Tunnel, Training, Broadcast, Spectate, Offline, NoUi, Tourney,
       // Debug options
       Tests, Stdout, FakeUi, Dummy, StrictVersion,
       // Special options
       NoFork, AppDir, SessionId );


namespace option { class Option; }

struct OptionsMessage : public SerializableSequence
{
    size_t operator[] ( const Options& opt ) const
    {
        auto it = options.find ( ( size_t ) opt.value );

        if ( it == options.end() )
            return 0;
        else
            return it->second.count;
    }

    void set ( const Options& opt, size_t count, const std::string& arg = "" )
    {
        if ( count == 0 )
            options.erase ( opt.value );
        else
            options[opt.value] = Opt ( count, arg );
    }

    const std::string& arg ( const Options& opt ) const
    {
        static const std::string EmptyString = "";

        auto it = options.find ( ( size_t ) opt.value );

        if ( it == options.end() )
            return EmptyString;
        else
            return it->second.arg;
    }

    OptionsMessage ( const std::vector<option::Option>& opt );

    PROTOCOL_MESSAGE_BOILERPLATE ( OptionsMessage, options )

private:

    struct Opt
    {
        size_t count;
        std::string arg;

        Opt() {}
        Opt ( size_t count, const std::string& arg = "" ) : count ( count ), arg ( arg ) {}

        CEREAL_CLASS_BOILERPLATE ( count, arg )
    };

    std::unordered_map<size_t, Opt> options;
};


struct Main
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Timer::Owner
{
    OptionsMessage options;

    ClientMode clientMode;

    IpAddrPort address;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;

    uint64_t pendingSocketTimeout = DEFAULT_PENDING_TIMEOUT;

    TimerPtr stopTimer;

    Logger syncLog;


    void pushPendingSocket ( const SocketPtr& socket )
    {
        LOG ( "socket=%08x", socket.get() );

        TimerPtr timer ( new Timer ( this ) );
        timer->start ( pendingSocketTimeout );

        pendingSockets[socket.get()] = socket;
        pendingSocketTimers[socket.get()] = timer;
        pendingTimerToSocket[timer.get()] = socket.get();
    }

    SocketPtr popPendingSocket ( Socket *socketPtr )
    {
        LOG ( "socket=%08x", socketPtr );

        auto it = pendingSockets.find ( socketPtr );

        if ( it == pendingSockets.end() )
            return 0;

        ASSERT ( pendingSocketTimers.find ( socketPtr ) != pendingSocketTimers.end() );

        SocketPtr socket = it->second;
        Timer *timerPtr = pendingSocketTimers[socketPtr].get();

        ASSERT ( pendingTimerToSocket.find ( timerPtr ) != pendingTimerToSocket.end() );

        pendingTimerToSocket.erase ( timerPtr );
        pendingSocketTimers.erase ( socketPtr );
        pendingSockets.erase ( socketPtr );

        return socket;
    }

    void expirePendingSocketTimer ( Timer *timerPtr )
    {
        LOG ( "timer=%08x", timerPtr );

        auto it = pendingTimerToSocket.find ( timerPtr );

        if ( it == pendingTimerToSocket.end() )
            return;

        LOG ( "socket=%08x", it->second );

        ASSERT ( pendingSockets.find ( it->second ) != pendingSockets.end() );
        ASSERT ( pendingSocketTimers.find ( it->second ) != pendingSocketTimers.end() );

        pendingSocketTimers.erase ( it->second );
        pendingSockets.erase ( it->second );
        pendingTimerToSocket.erase ( timerPtr );
    }


    Main() : procMan ( this ) {}

    Main ( const ClientMode& clientMode ) : clientMode ( clientMode ), procMan ( this ) {}

private:

    std::unordered_map<Socket *, SocketPtr> pendingSockets;

    std::unordered_map<Socket *, TimerPtr> pendingSocketTimers;

    std::unordered_map<Timer *, Socket *> pendingTimerToSocket;
};


struct AutoManager
{
    AutoManager()
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
    }

    template<typename T>
    AutoManager ( T *main,
                  const void *window = 0,                           // Window to match, 0 to match all
                  const std::unordered_set<uint32_t>& keys = {},    // VK codes to match, empty to match all
                  uint8_t options = 0 )                             // Keyboard event hooking options
        : AutoManager()
    {
        KeyboardManager::get().hook ( main, window, keys, options );
    }

    ~AutoManager()
    {
        KeyboardManager::get().unhook();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }
};
