#include "PaletteManager.hpp"
#include "DllAsmHacks.hpp"
#include "Logger.hpp"
#include "CharacterSelect.hpp"
#include "ProcessManager.hpp"

using namespace std;


static array<unordered_map<uint32_t, PaletteManager>, 2> palMans;


namespace AsmHacks
{

void colorLoadCallback ( uint32_t player, uint32_t chara, uint32_t *allPaletteData )
{
    ASSERT ( player == 1 || player == 2 );

    const string charaName = getShortCharaName ( chara );

    LOG ( "player=%d; chara=[%u]%s; allPaletteData=%08X", player, chara, charaName, allPaletteData );

    if ( palMans[player - 1].find ( chara ) == palMans[player - 1].end() )
    {
        palMans[player - 1][chara].cache ( ( const uint32_t * ) allPaletteData );
        palMans[player - 1][chara].load ( ProcessManager::appDir + PALETTES_FOLDER, charaName );
    }

    palMans[player - 1][chara].apply ( allPaletteData );
}

void colorLoadCallback ( uint32_t player, uint32_t chara, uint32_t palette, uint32_t *singlePaletteData )
{
    ASSERT ( player == 1 || player == 2 );

    const string charaName = getShortCharaName ( chara );

    LOG ( "player=%d; chara=[%u]%s; palette=%u; singlePaletteData=%08X",
          player, chara, charaName, palette, singlePaletteData );

    // This really shouldn't happen since the callback above should always happen first.
    // But in case we haven't loaded the custom palette data, just abort here.
    if ( palMans[player - 1].find ( chara ) == palMans[player - 1].end() )
        return;

    palMans[player - 1][chara].apply ( palette, singlePaletteData );
}

} // namespace AsmHacks
