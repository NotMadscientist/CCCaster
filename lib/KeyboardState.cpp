#include "KeyboardState.h"

#include <windows.h>

using namespace std;


unordered_map<uint32_t, bool> KeyboardState::states;

unordered_map<uint32_t, bool> KeyboardState::previous;

unordered_map<uint32_t, uint64_t> KeyboardState::pressedTimestamp;

uint32_t KeyboardState::repeatTimer = 0;

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
    pressedTimestamp.clear();
}

void KeyboardState::update()
{
    previous = states;

    for ( auto& kv : states )
    {
        kv.second = getKeyState ( kv.first );

        if ( kv.second && !wasDown ( kv.first ) )
        {
            pressedTimestamp[kv.first] = TimerManager::get().getNow ( true );
        }
        else if ( !kv.second && wasDown ( kv.first ) )
        {
            pressedTimestamp.erase ( kv.first );
        }
    }

    ++repeatTimer;
}

bool KeyboardState::isDown ( uint32_t vkCode )
{
    auto it = states.find ( vkCode );

    if ( it != states.end() )
        return it->second;

    if ( ( states[vkCode] = getKeyState ( vkCode ) ) )
    {
        pressedTimestamp[vkCode] = TimerManager::get().getNow ( true );
        return true;
    }

    return false;
}
