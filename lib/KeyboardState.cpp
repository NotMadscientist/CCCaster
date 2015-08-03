#include "KeyboardState.hpp"

#include <windows.h>

using namespace std;


unordered_map<uint32_t, bool> KeyboardState::_state;

unordered_map<uint32_t, bool> KeyboardState::_previous;

unordered_map<uint32_t, uint64_t> KeyboardState::_pressedTimestamp;

uint32_t KeyboardState::_repeatTimer = 0;

void *KeyboardState::windowHandle = 0;


static inline bool getKeyState ( uint32_t vkCode )
{
    if ( KeyboardState::windowHandle && KeyboardState::windowHandle != ( void * ) GetForegroundWindow() )
        return false;

    return ( GetKeyState ( vkCode ) & 0x80 );
}


void KeyboardState::clear()
{
    _state.clear();
    _previous.clear();
    _pressedTimestamp.clear();
}

void KeyboardState::update()
{
    const uint64_t now = TimerManager::get().getNow ( true );

    _previous = _state;

    for ( auto& kv : _state )
    {
        kv.second = getKeyState ( kv.first );

        if ( kv.second && !wasDown ( kv.first ) )
        {
            _pressedTimestamp[kv.first] = now;
        }
        else if ( !kv.second && wasDown ( kv.first ) )
        {
            _pressedTimestamp.erase ( kv.first );
        }
    }

    ++_repeatTimer;
}

bool KeyboardState::isDown ( uint32_t vkCode )
{
    const auto it = _state.find ( vkCode );

    if ( it != _state.end() )
        return it->second;

    if ( ( _state[vkCode] = getKeyState ( vkCode ) ) )
    {
        _pressedTimestamp[vkCode] = TimerManager::get().getNow ( true );
        return true;
    }

    return false;
}
