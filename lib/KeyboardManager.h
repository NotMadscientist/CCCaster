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

    // If we're hooked
    bool hooked = false;

    // Socket to receive KeyboardEvent messages
    SocketPtr recvSocket;

    // Socket callbacks
    void acceptEvent ( Socket *socket ) override {}
    void connectEvent ( Socket *socket ) override {}
    void disconnectEvent ( Socket *socket ) override {}
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;

public:

    // Window to match for keyboard events, 0 to match all, NOT safe to modify when hooked!
    const void *keyboardWindow = 0;

    // VK codes to match for keyboard events, empty to match all, NOT safe to modify when hooked!
    std::unordered_set<uint32_t> matchedKeys;

    // VK codes to IGNORE for keyboard events, NOT safe to modify when hooked!
    std::unordered_set<uint32_t> ignoredKeys;

    // Hook keyboard events
    void hook ( Owner *owner, bool externalHook = false );

    // Unhook keyboard events
    void unhook();

    // Indicates if keyboard events are hooked
    bool isHooked() const;

    // Get the singleton instance
    static KeyboardManager& get();
};
