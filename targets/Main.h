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
    IpAddrPort address;

    ClientType::Enum clientType = ClientType::Unknown;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;

    TimerPtr timer;


    CommonMain() : procMan ( this ) {}

    CommonMain ( const IpAddrPort& address )
        : address ( address )
        , clientType ( address.addr.empty() ? ClientType::Host : ClientType::Client )
        , procMan ( this ) {}

    bool isHost() const { return ( clientType == ClientType::Host ); }
    bool isClient() const { return ( clientType == ClientType::Client ); }
};
