#include "EventManager.h"

Mutex eventMutex;


EventManager::EventManager()
{
}

Socket *EventManager::serverSocket ( Socket::Owner *owner, NL::Protocol protocol, unsigned port )
{
}

Socket *EventManager::clientSocket ( Socket::Owner *owner, NL::Protocol protocol, const IpAddrPort& address )
{
}

void EventManager::start()
{
}
