#pragma once

#include "EventManager.h"
#include "ProcessManager.h"
#include "SocketManager.h"
#include "Socket.h"
#include "TimerManager.h"
#include "Timer.h"
#include "KeyboardManager.h"
#include "IpAddrPort.h"
#include "Options.h"

#include <unordered_set>


// Log file that contains all the data needed to keep games in sync
#define SYNC_LOG_FILE FOLDER "sync.log"

// Controller mappings file extension
#define MAPPINGS_EXT ".mappings"


struct Main
        : public ProcessManager::Owner
        , public Socket::Owner
        , public Timer::Owner
{
    OptionsMessage options;

    ClientMode clientMode;

    IpAddrPort address;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;

    TimerPtr stopTimer;

    Logger syncLog;


    Main() : procMan ( this ) {}

    Main ( const ClientMode& clientMode ) : clientMode ( clientMode ), procMan ( this ) {}
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
                  const std::unordered_set<uint32_t>& ignore = {} ) // VK codes to specifically IGNORE
        : AutoManager()
    {
        KeyboardManager::get().keyboardWindow = window;
        KeyboardManager::get().matchedKeys = keys;
        KeyboardManager::get().ignoredKeys = ignore;
        KeyboardManager::get().hook ( main, true );
    }

    ~AutoManager()
    {
        KeyboardManager::get().unhook();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }
};
