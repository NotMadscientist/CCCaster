#include "AsmHacks.h"
#include "Messages.h"
#include "NetplayManager.h"
#include "CharacterSelect.h"

#include <windows.h>

using namespace std;


static int memwrite ( void *dst, const void *src, size_t len )
{
    DWORD old, tmp;

    if ( !VirtualProtect ( dst, len, PAGE_READWRITE, &old ) )
        return GetLastError();

    memcpy ( dst, src, len );

    if ( !VirtualProtect ( dst, len, old, &tmp ) )
        return GetLastError();

    return 0;
}


namespace AsmHacks
{

uint32_t currentMenuIndex = 0;

uint32_t menuConfirmState = 0;

uint32_t roundStartCounter = 0;

uint32_t *autoReplaySaveStatePtr = 0;


int Asm::write() const
{
    backup.resize ( bytes.size() );
    memcpy ( &backup[0], addr, backup.size() );
    return memwrite ( addr, &bytes[0], bytes.size() );
}

int Asm::revert() const
{
    return memwrite ( addr, &backup[0], backup.size() );
}

} // namespace AsmHacks


// This is here because this constructor should only be called when running in the DLL, ie MBAA's memory space
InitialGameState::InitialGameState ( IndexedFrame indexedFrame, uint8_t netplayState, bool isTraining )
    : indexedFrame ( indexedFrame )
    , stage ( *CC_STAGE_SELECTOR_ADDR )
    , netplayState ( netplayState )
    , isTraining ( isTraining )
{
    chara[0] = ( uint8_t ) * CC_P1_CHARACTER_ADDR;
    chara[1] = ( uint8_t ) * CC_P2_CHARACTER_ADDR;

    moon[0] = ( uint8_t ) * CC_P1_MOON_SELECTOR_ADDR;
    moon[1] = ( uint8_t ) * CC_P2_MOON_SELECTOR_ADDR;

    color[0] = ( uint8_t ) * CC_P1_COLOR_SELECTOR_ADDR;
    color[1] = ( uint8_t ) * CC_P2_COLOR_SELECTOR_ADDR;
}
