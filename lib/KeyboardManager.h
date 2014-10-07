#pragma once

#include "Protocol.h"
#include "Socket.h"

#include <unordered_set>


struct KeyboardEvent : public SerializableMessage
{
    int vkCode = 0;
    uint8_t isDown = 0;

    KeyboardEvent ( int vkCode, uint8_t isDown ) : vkCode ( vkCode ), isDown ( isDown ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( KeyboardEvent, vkCode, isDown )
};


struct KeyboardManager
{
    // Window to match for keyboard events
    const void *window = 0;

    // VK codes to match for keyboard events
    std::unordered_set<int> keys;

    // Hook keyboard events and send messages to the given socket
    void hook ( const SocketPtr& eventSocket );

    // Unhook keyboard events
    void unhook();

    // Get the singleton instance
    static KeyboardManager& get();
};
