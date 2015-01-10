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

uint32_t *autoReplaySaveStatePtr = 0;

uint8_t enableEscapeToExit = true;

uint8_t sfxFilterArray[CC_SFX_ARRAY_LEN] = { 0 };


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


// The following constructors should only be called when running in the DLL, ie MBAA's memory space
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

SyncHash::SyncHash ( IndexedFrame indexedFrame )
{
    this->indexedFrame = indexedFrame;

    char data [ sizeof ( uint32_t ) * 3 + CC_RNGSTATE3_SIZE ];

    ( * ( uint32_t * ) &data[0] ) = *CC_RNGSTATE0_ADDR;
    ( * ( uint32_t * ) &data[4] ) = *CC_RNGSTATE1_ADDR;
    ( * ( uint32_t * ) &data[8] ) = *CC_RNGSTATE2_ADDR;
    memcpy ( &data[12], CC_RNGSTATE3_ADDR, CC_RNGSTATE3_SIZE );

    getMD5 ( data, sizeof ( data ), hash );

    if ( *CC_GAME_MODE_ADDR != CC_GAME_MODE_IN_GAME )
    {
        memset ( &chara[0], 0, sizeof ( CharaHash ) );
        memset ( &chara[1], 0, sizeof ( CharaHash ) );
        chara[0].chara = ( uint16_t ) * CC_P1_CHARACTER_ADDR;
        chara[0].moon  = ( uint16_t ) * CC_P1_MOON_SELECTOR_ADDR;
        chara[1].chara = ( uint16_t ) * CC_P2_CHARACTER_ADDR;
        chara[1].moon  = ( uint16_t ) * CC_P2_MOON_SELECTOR_ADDR;
        return;
    }

#define SAVE_CHARA(N)                                                                           \
    chara[N-1].seq          = *CC_P ## N ## _SEQUENCE_ADDR;                                     \
    chara[N-1].seqState     = *CC_P ## N ## _SEQ_STATE_ADDR;                                    \
    chara[N-1].health       = *CC_P ## N ## _HEALTH_ADDR;                                       \
    chara[N-1].redHealth    = *CC_P ## N ## _RED_HEALTH_ADDR;                                   \
    chara[N-1].meter        = *CC_P ## N ## _METER_ADDR;                                        \
    chara[N-1].heat         = *CC_P ## N ## _HEAT_ADDR;                                         \
    chara[N-1].guardBar     = ( *CC_INTRO_STATE_ADDR ? 0 : *CC_P ## N ## _GUARD_BAR_ADDR );     \
    chara[N-1].guardQuality = *CC_P ## N ## _GUARD_QUALITY_ADDR;                                \
    chara[N-1].x            = *CC_P ## N ## _X_POSITION_ADDR;                                   \
    chara[N-1].y            = *CC_P ## N ## _Y_POSITION_ADDR;                                   \
    chara[N-1].chara = ( uint16_t ) * CC_P ## N ## _CHARACTER_ADDR;                             \
    chara[N-1].moon  = ( uint16_t ) * CC_P ## N ## _MOON_SELECTOR_ADDR;

    SAVE_CHARA ( 1 )
    SAVE_CHARA ( 2 )
}
