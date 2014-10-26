#pragma once

#include "EventManager.h"
#include "ProcessManager.h"
#include "SocketManager.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include "ControllerManager.h"
#include "Controller.h"
#include "TimerManager.h"
#include "Timer.h"
#include "KeyboardManager.h"
#include "IpAddrPort.h"
#include "Messages.h"

#include <unordered_set>
#include <unordered_map>


struct CommonMain
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Timer::Owner
{
    ClientMode clientMode;

    IpAddrPort address;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;

    std::unordered_map<Socket *, SocketPtr> pendingSockets;


    CommonMain() : procMan ( this ) {}

    CommonMain ( const ClientMode& clientMode ) : clientMode ( clientMode ), procMan ( this ) {}
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
        KeyboardManager::get().hook ( main, window, keys, options );
    }

    ~AutoManager()
    {
        KeyboardManager::get().unhook();
        ControllerManager::get().deinitialize();
        SocketManager::get().deinitialize();
        TimerManager::get().deinitialize();
    }
};
