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
    ClientMode clientMode;

    ProcessManager procMan;

    SocketPtr serverCtrlSocket, ctrlSocket;

    SocketPtr serverDataSocket, dataSocket;

    TimerPtr stopTimer;


    CommonMain() : procMan ( this ) {}

    CommonMain ( const ClientMode& clientMode ) : clientMode ( clientMode ), procMan ( this ) {}

    bool isHost() const { return ( clientMode == ClientMode::Host ); }
    bool isClient() const { return ( clientMode == ClientMode::Client ); }
    bool isSpectate() const { return ( clientMode == ClientMode::Spectate ); }
    bool isBroadcast() const { return ( clientMode == ClientMode::Broadcast ); }
    bool isOffline() const { return ( clientMode == ClientMode::Offline ); }
    bool isNetplay() const { return ( clientMode == ClientMode::Host || clientMode == ClientMode::Client ); }
    bool isLocal() const { return ( clientMode == ClientMode::Broadcast || clientMode == ClientMode::Offline ); }
};
