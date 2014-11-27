#include "AsmHacks.h"
#include "Messages.h"

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

array<uint32_t *, 2> charaSelectModePtrs = { 0, 0 };

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


InitialGameState::InitialGameState ( bool isTraining )
{
    stage = *CC_STAGE_SELECTOR_ADDR;

    chara[0] = *CC_P1_CHARA_SELECTOR_ADDR;
    chara[1] = *CC_P2_CHARA_SELECTOR_ADDR;

    moon[0] = ( uint8_t ) * CC_P1_MOON_SELECTOR_ADDR;
    moon[1] = ( uint8_t ) * CC_P2_MOON_SELECTOR_ADDR;

    color[0] = ( uint8_t ) * CC_P1_COLOR_SELECTOR_ADDR;
    color[1] = ( uint8_t ) * CC_P2_COLOR_SELECTOR_ADDR;

    if ( *CC_GAME_MODE_ADDR == CC_GAME_MODE_CHARA_SELECT )
    {
        if ( AsmHacks::charaSelectModePtrs[0] == 0 )
            charaSelectMode[0] = 0;
        else
            charaSelectMode[0] = ( uint8_t ) * AsmHacks::charaSelectModePtrs[0];

        if ( AsmHacks::charaSelectModePtrs[1] == 0 )
            charaSelectMode[1] = 0;
        else
            charaSelectMode[1] = ( uint8_t ) * AsmHacks::charaSelectModePtrs[1];

        initialMode = InitialGameState::CharaSelect;
    }
    else if ( isTraining )
    {
        initialMode = InitialGameState::TrainingMode;
    }
    else
    {
        initialMode = InitialGameState::VersusMode;
    }
}
