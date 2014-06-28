#include "ReliableUdp.h"

using namespace std;

void ReliableUdp::ProxyOwner::sendGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
}

void ReliableUdp::ProxyOwner::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
}

void ReliableUdp::ProxyOwner::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
}

ReliableUdp::ReliableUdp ( Socket::Owner *owner, unsigned port )
    : Socket ( &proxy, port, Protocol::UDP ), proxy ( owner ), gbn ( this )
{
}

ReliableUdp::ReliableUdp ( Socket::Owner *owner, const string& address, unsigned port )
    : Socket ( &proxy, address, port, Protocol::UDP ), proxy ( owner ), gbn ( this )
{
}

ReliableUdp::~ReliableUdp()
{
    disconnect();
}

void ReliableUdp::disconnect()
{
    Socket::disconnect();
}

shared_ptr<Socket> ReliableUdp::listen ( Socket::Owner *owner, unsigned port )
{
    return 0;
}

shared_ptr<Socket> ReliableUdp::connect ( Socket::Owner *owner, const string& address, unsigned port )
{
    return 0;
}

shared_ptr<Socket> ReliableUdp::accept ( Socket::Owner *owner )
{
    return 0;
}

void ReliableUdp::send ( Serializable *message, const IpAddrPort& address )
{
}

void ReliableUdp::send ( const MsgPtr& msg, const IpAddrPort& address )
{
}
