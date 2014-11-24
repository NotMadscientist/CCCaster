#include "KeyboardState.h"

#include <windows.h>

using namespace std;


unordered_map<uint32_t, bool> KeyboardState::states;

unordered_map<uint32_t, bool> KeyboardState::previous;

void *KeyboardState::windowHandle = 0;


static inline bool getKeyState ( uint32_t vkCode )
{
    if ( KeyboardState::windowHandle && KeyboardState::windowHandle != ( void * ) GetForegroundWindow() )
        return false;

    return ( GetKeyState ( vkCode ) & 0x80 );
}


void KeyboardState::clear()
{
    states.clear();
    previous.clear();
}

void KeyboardState::update()
{
    previous = states;

    for ( auto& kv : states )
        kv.second = getKeyState ( kv.first );
}

bool KeyboardState::isDown ( uint32_t vkCode )
{

    auto it = states.find ( vkCode );

    if ( it != states.end() )
        return it->second;

    return states[vkCode] = getKeyState ( vkCode );
}
