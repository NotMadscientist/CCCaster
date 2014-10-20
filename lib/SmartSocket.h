#pragma once

#include "Socket.h"


class SmartSocket
{
public:

    // Listen for connections on the given port.
    // Opens a regular TCP server socket, but also checks a server for UDP tunnel connections.
    static SocketPtr listen ( Socket::Owner *owner, uint16_t port );

    // Connect to the given address and port.
    // Tries to connect using the given protocol, with fallback to UDP tunnel.
    static SocketPtr connect ( Socket::Owner *owner, const IpAddrPort& address, Socket::Protocol protocol );
};
