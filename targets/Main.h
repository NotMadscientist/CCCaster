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
#include "IpAddrPort.h"
#include "Messages.h"


struct CommonMain
        : public ProcessManager::Owner
        , public Socket::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , public Timer::Owner
{
    ClientType clientType;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;


    CommonMain() : procMan ( this ) {}

    CommonMain ( const ClientType& clientType ) : clientType ( clientType ), procMan ( this ) {}

    bool isHost() const { return ( clientType == ClientType::Host ); }
    bool isClient() const { return ( clientType == ClientType::Client ); }
    bool isLocal() const { return ( clientType == ClientType::Broadcast || clientType == ClientType::Offline ); }
};
