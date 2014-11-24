#pragma once

#include <unordered_map>


class KeyboardState
{
    static std::unordered_map<uint32_t, bool> states, previous;

public:

    static void *windowHandle;

    static void clear();

    static void update ();

    static bool isDown ( uint32_t vkCode );

    static bool wasDown ( uint32_t vkCode )
    {
        const auto it = previous.find ( vkCode );

        if ( it != previous.end() )
            return it->second;

        return ( it != previous.end() && it->second );
    }

    static inline bool isPressed ( uint32_t vkCode )
    {
        return ( isDown ( vkCode ) && !wasDown ( vkCode ) );
    }

    static inline bool isReleased ( uint32_t vkCode )
    {
        return ( !isDown ( vkCode ) && wasDown ( vkCode ) );
    }
};
