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
#include "Messages.h"


struct CommonMain
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Timer::Owner
{
    ClientType::Enum clientType;
    ProcessManager procMan;
    SocketPtr serverSocket, ctrlSocket, dataSocket;
    TimerPtr timer;

    inline CommonMain() : clientType ( ClientType::Unknown ), procMan ( this ) {}

    inline bool isHost() const { return ( clientType == ClientType::Host ); }
};
