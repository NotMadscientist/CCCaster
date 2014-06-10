#pragma once

#include "Thread.h"

#include <netlink/socket.h>
#include <netlink/socket_group.h>

#include <memory>

class EventManager
{
    NL::SocketGroup socketGroup;

    THREAD ( SocketThread, EventManager ) socketThread;

    THREAD ( ReaperThread, EventManager ) reaperThread;

    THREAD ( TimerThread, EventManager ) timerThread;

public:

    EventManager();

    Socket *serverSocket ( Socket::Owner *owner, NL::Protocol protocol, unsigned port );

    Socket *clientSocket ( Socket::Owner *owner, NL::Protocol protocol, const IpAddrPort& address );

    void start();
};
