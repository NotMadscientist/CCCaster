#include "KeyboardManager.h"
#include "DllHacks.h"

#include <windows.h>

using namespace std;


LRESULT CALLBACK keyboardCallback ( int code, WPARAM wParam, LPARAM lParam )
{
    while ( code == HC_ACTION )
    {
        const KeyboardManager& km = KeyboardManager::get();

        // Ignore key if socket is unconnected
        if ( !km.sendSocket || !km.sendSocket->isConnected() )
            break;

        // Ignore key if the window does not match
        if ( km.keyboardWindow && ( GetForegroundWindow() != km.keyboardWindow ) )
            break;

        const uint32_t vkCode = wParam;

        // Ignore key if the VK code is not matched
        if ( !km.matchedKeys.empty() && ( km.matchedKeys.find ( vkCode ) == km.matchedKeys.end() ) )
            break;

        // Ignore key if it is explicitly ignored
        if ( km.ignoredKeys.find ( vkCode ) != km.ignoredKeys.end() )
            break;

        const uint32_t scanCode = ( lParam >> 16 ) & 127;
        const bool isExtended = ( lParam >> 24 ) & 1;
        const bool isDown = ( lParam >> 31 ) & 1;

        // Send KeyboardEvent message and return 1 to eat the keyboard event
        km.sendSocket->send ( new KeyboardEvent ( vkCode, scanCode, isExtended, isDown ) );
        return 1;
    }

    return CallNextHookEx ( 0, code, wParam, lParam );
}


void KeyboardManager::hookImpl()
{
    DllHacks::keyboardManagerHooked = true;
}

void KeyboardManager::unhookImpl()
{
    DllHacks::keyboardManagerHooked = false;
}
