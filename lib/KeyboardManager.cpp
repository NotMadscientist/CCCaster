#include "KeyboardManager.h"
#include "UdpSocket.h"
#include "Logger.h"

using namespace std;


void KeyboardManager::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    ASSERT ( socket == recvSocket.get() );

    if ( !msg.get() || msg->getMsgType() != MsgType::KeyboardEvent )
    {
        LOG ( "Unexpected '%s'", msg );
        return;
    }

    const uint32_t vkCode = msg->getAs<KeyboardEvent>().vkCode;
    const uint32_t scanCode = msg->getAs<KeyboardEvent>().scanCode;
    const bool isExtended = msg->getAs<KeyboardEvent>().isExtended;
    const bool isDown = msg->getAs<KeyboardEvent>().isDown;

    LOG ( "vkCode=%u; scanCode=%u; isExtended=%u; isDown=%u", vkCode, scanCode, isExtended, isDown );

    if ( owner )
        owner->keyboardEvent ( vkCode, scanCode, isExtended, isDown );
}

void KeyboardManager::hook ( Owner *owner )
{
    if ( recvSocket || sendSocket )
        return;

    LOG ( "Hooking keyboard manager" );

    this->owner = owner;

    recvSocket = UdpSocket::bind ( this, 0 );
    sendSocket = UdpSocket::bind ( 0, { "127.0.0.1", recvSocket->address.port } );

    hookImpl();

    LOG ( "Hooked keyboard manager" );
}

void KeyboardManager::unhook()
{
    LOG ( "Unhooking keyboard manager" );

    unhookImpl();

    sendSocket.reset();
    recvSocket.reset();

    this->owner = 0;

    LOG ( "Unhooked keyboard manager" );
}

KeyboardManager& KeyboardManager::get()
{
    static KeyboardManager instance;
    return instance;
}
