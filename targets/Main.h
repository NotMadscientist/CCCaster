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

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>


// Set of command line options
ENUM ( Options, Help, Dummy, Tests, Stdout, NoFork, NoUi, Strict, Training, Broadcast, Spectate, Offline, Dir );

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

    std::unordered_map<Socket *, SocketPtr> pendingSockets;


    Main() : procMan ( this ) {}

    Main ( const ClientMode& clientMode ) : clientMode ( clientMode ), procMan ( this ) {}
};


struct AutoManager
{
    template<typename T>
    AutoManager ( T *main )
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();
        ControllerManager::get().initialize ( main );
    }

    template<typename T>
    AutoManager ( T *main,
                  const void *window,                       // Window to match for keyboard events, 0 to match all
                  const std::unordered_set<int>& keys = {}, // VK codes to match for keyboard events, empty to match all
                  uint8_t options = 0 )                     // Keyboard event hooking options
        : AutoManager ( main )
    {
        // KeyboardManager::get().hook ( main, window, keys, options );
    }

    ~AutoManager()
    {
        KeyboardManager::get().unhook();
        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }
};
