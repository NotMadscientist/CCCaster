#include "SmartSocket.h"

using namespace std;

void SmartSocket::tcpAccepted ( const string& addrPort )
{
    // accepted ( bytes, len, MAP_ADDRESS ( addrPort ) );
}

void SmartSocket::tcpConnected ( const string& addrPort )
{
    // connected ( bytes, len, MAP_ADDRESS ( addrPort ) );
}

void SmartSocket::tcpDisconnected ( const string& addrPort )
{
    // disconnected ( bytes, len, MAP_ADDRESS ( addrPort ) );
}

void SmartSocket::tcpReceived ( char *bytes, size_t len, const string& addrPort )
{
    // receivedPrimary ( bytes, len, MAP_ADDRESS ( addrPort ) );
}

void SmartSocket::udpReceived ( char *bytes, size_t len, const string& addr, unsigned port )
{
    // receivedSecondary ( bytes, len, MAP_ADDRESS ( getAddrPort ( addr, port ) ) );
}

SmartSocket::SmartSocket()
{
}

SmartSocket::~SmartSocket()
{
}

void SmartSocket::connect ( const string& addr, unsigned port )
{
    Socket::tcpConnect ( addr, port );
    Socket::udpConnect ( addr, port );
}
