#include "AsmHacks.h"
#include "Messages.h"
#include "NetplayManager.h"

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


InitialGameState::InitialGameState ( uint32_t index, uint8_t netplayState )
    : index ( index )
    , stage ( *CC_STAGE_SELECTOR_ADDR )
    , netplayState ( netplayState )
{
    charaSelector[0] = ( uint8_t ) * CC_P1_CHARA_SELECTOR_ADDR;
    charaSelector[1] = ( uint8_t ) * CC_P2_CHARA_SELECTOR_ADDR;

    character[0] = ( uint8_t ) * CC_P1_CHARACTER_ADDR;
    character[1] = ( uint8_t ) * CC_P2_CHARACTER_ADDR;

    moon[0] = ( uint8_t ) * CC_P1_MOON_SELECTOR_ADDR;
    moon[1] = ( uint8_t ) * CC_P2_MOON_SELECTOR_ADDR;

    color[0] = ( uint8_t ) * CC_P1_COLOR_SELECTOR_ADDR;
    color[1] = ( uint8_t ) * CC_P2_COLOR_SELECTOR_ADDR;

    if ( *CC_P1_RANDOM_COLOR_ADDR )
        isRandomColor |= 0x01;

    if ( *CC_P2_RANDOM_COLOR_ADDR )
        isRandomColor |= 0x02;

    if ( netplayState == NetplayState::CharaSelect )
    {
        charaSelectMode[0] = ( uint8_t ) * CC_P1_SELECTOR_MODE_ADDR;
        charaSelectMode[1] = ( uint8_t ) * CC_P2_SELECTOR_MODE_ADDR;
    }
}
