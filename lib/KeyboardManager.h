#pragma once

#include "Protocol.h"
#include "Socket.h"

#include <unordered_set>


struct KeyboardEvent : public SerializableMessage
{
    uint32_t vkCode = 0;
    uint32_t scanCode = 0;
    uint8_t isExtended = 0;
    uint8_t isDown = 0;

    KeyboardEvent ( uint32_t vkCode, uint32_t scanCode, uint8_t isExtended, uint8_t isDown )
        : vkCode ( vkCode ), scanCode ( scanCode ), isExtended ( isExtended ), isDown ( isDown ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( KeyboardEvent, vkCode, scanCode, isExtended, isDown )
};


class KeyboardManager : public Socket::Owner
{
public:

    struct Owner
    {
        virtual void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown ) = 0;
    };

    Owner *owner = 0;

private:

    // Socket to receive KeyboardEvent messages
    SocketPtr recvSocket;

    // Socket callbacks
    void acceptEvent ( Socket *socket ) override {}
    void connectEvent ( Socket *socket ) override {}
    void disconnectEvent ( Socket *socket ) override {}
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Hook keyboard events
    void hook ( Owner *owner,
                const void *window = 0,                         // Window to match for keyboard events, 0 to match all
                const std::unordered_set<uint32_t>& keys = {},  // VK codes to match, empty to match all
                uint8_t options = 0 );                          // Keyboard event hooking options

    // Unhook keyboard events
    void unhook();

    // Indicates if the keyboard events are already hooked
    bool isHooked() const;

    // Get the singleton instance
    static KeyboardManager& get();
};
