#include "DllAsmHacks.h"
#include "Messages.h"
#include "DllNetplayManager.h"
#include "CharacterSelect.h"
#include "Logger.h"

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

uint8_t enableEscapeToExit = true;

uint8_t sfxFilterArray[CC_SFX_ARRAY_LEN] = { 0 };

uint8_t sfxMuteArray[CC_SFX_ARRAY_LEN] = { 0 };

uint32_t numLoadedColors = 0;


static unordered_map<uint32_t, pair<uint32_t, uint32_t>> teamCharas =
{
    // TODO verify the order of loading
    {  4, {  5,  6 } }, // Maids -> Hisui, Kohaku
    { 34, { 14, 20 } }, // NekoMech -> Neko, M.Hisui
    { 35, {  6, 14 } }, // KohaMech -> Kohaku, M.Hisui
};

extern "C" void charaSelectColorCb()
{
    uint32_t *edi;

    asm ( "movl %%edi,%0" : "=r" ( edi ) );

    Sleep ( 20 ); // This is code that was replaced

    uint32_t *ptrBase = ( uint32_t * ) 0x74D808;

    if ( ! *ptrBase )
        return;

    uint32_t *ptr1 = ( uint32_t * ) ( *ptrBase + 0x1AC ); // P1 color table reference
    uint32_t *ptr2 = ( uint32_t * ) ( *ptrBase + 0x388 ); // P2 color table reference

    LOG ( "edi=%08X; ptr1=%08X; *ptr1=%08X; ptr2=%08X; *ptr2=%08X", edi, ptr1, *ptr1, ptr2, *ptr2 );

    if ( edi + 1 == ptr1 && *ptr1 )
    {
        const uint32_t chara1 = *CC_P1_CHARACTER_ADDR;
        const auto& team1 = teamCharas.find ( chara1 );

        if ( team1 != teamCharas.end() )
        {
            LOG ( "Team1: %s and %s",
                  getShortCharaName ( team1->second.first ),
                  getShortCharaName ( team1->second.second ) );

            // TODO how to handle?
        }
        else
        {
            colorLoadCallback ( 1, chara1, ( ( uint32_t * ) *ptr1 ) + 1 );
        }
    }
    else if ( edi + 1 == ptr2 && *ptr2 )
    {
        const uint32_t chara2 = *CC_P2_CHARACTER_ADDR;
        const auto& team2 = teamCharas.find ( chara2 );

        if ( team2 != teamCharas.end() )
        {
            LOG ( "Team2: %s and %s",
                  getShortCharaName ( team2->second.first ),
                  getShortCharaName ( team2->second.second ) );

            // TODO how to handle?
        }
        else
        {
            colorLoadCallback ( 2, chara2, ( ( uint32_t * ) *ptr2 ) + 1 );
        }
    }
}

static void loadingStateColorCb2 ( uint32_t *singlePaletteData )
{
    const uint32_t chara1 = *CC_P1_CHARACTER_ADDR;
    const uint32_t chara2 = *CC_P2_CHARACTER_ADDR;

    const auto& team1 = teamCharas.find ( chara1 );
    const auto& team2 = teamCharas.find ( chara2 );

    const bool hasTeam1 = ( team1 != teamCharas.end() );
    const bool hasTeam2 = ( team2 != teamCharas.end() );

    if ( hasTeam1 || hasTeam2 )
    {
        // uint32_t player;

        // if ( hasTeam1 )
        //     player = ( numLoadedColors < 2 ? 1 : 2 );
        // else
        //     player = ( numLoadedColors < 1 ? 1 : 2 );

        // uint32_t chara = ( player == 1 ? chara1 : chara2 );

        // if ( hasTeam1 && player == 1 )
        //     chara = ( numLoadedColors == 0 ? team1->second.first : team1->second.second );
        // else if ( hasTeam2 && player == 2 )
        //     chara = ( numLoadedColors == ( hasTeam1 ? 2 : 1 ) ? team1->second.first : team1->second.second );

        // colorLoadCallback (
        //     player,
        //     chara,
        //     * ( player == 1 ? CC_P1_COLOR_SELECTOR_ADDR : CC_P2_COLOR_SELECTOR_ADDR ),
        //     singlePaletteData );
    }
    else
    {
        colorLoadCallback (
            numLoadedColors + 1,
            ( numLoadedColors == 0 ? chara1 : chara2 ),
            * ( numLoadedColors == 0 ? CC_P1_COLOR_SELECTOR_ADDR : CC_P2_COLOR_SELECTOR_ADDR ),
            singlePaletteData );
    }

    ++numLoadedColors;
}

extern "C" void loadingStateColorCb()
{
    uint32_t *ebx, *esi;

    asm ( "movl %%ebx,%0" : "=r" ( ebx ) );
    asm ( "movl %%esi,%0" : "=r" ( esi ) );

    uint32_t *ptr = ( uint32_t * ) ( ( uint32_t ( esi ) << 10 ) + uint32_t ( ebx ) + 4 );

    LOG ( "ebx=%08X; esi=%08X; ptr=%08X", ebx, esi, ptr );

    loadingStateColorCb2 ( ptr );
}

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
