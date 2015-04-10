#include "PaletteManager.h"
#include "DllAsmHacks.h"
#include "Logger.h"
#include "CharacterSelect.h"
#include "ProcessManager.h"

using namespace std;


static array<unordered_map<uint32_t, PaletteManager>, 2> palMans;


namespace AsmHacks
{

void colorLoadCallback ( uint32_t player, uint32_t chara, uint32_t *allPaletteData )
{
    ASSERT ( player == 1 || player == 2 );

    const string name = getShortCharaName ( chara );

    LOG ( "player=%d; chara=[%u]%s; allPaletteData=%08X", player, chara, name, allPaletteData );

    if ( palMans[player - 1].find ( chara ) == palMans[player - 1].end() )
    {
        palMans[player - 1][chara].cache ( ( const uint32_t * ) allPaletteData );
        palMans[player - 1][chara].load ( ProcessManager::appDir + PALETTES_FOLDER + name + "_palettes.txt" );
    }

    palMans[player - 1][chara].apply ( allPaletteData );
}

void colorLoadCallback ( uint32_t player, uint32_t chara, uint32_t palette, uint32_t *singlePaletteData )
{
    ASSERT ( player == 1 || player == 2 );

    const string name = getShortCharaName ( chara );

    LOG ( "player=%d; chara=[%u]%s; palette=%u; singlePaletteData=%08X",
          player, chara, name, palette, singlePaletteData );

    // This really shouldn't happen since the callback above should always happen first.
    // But in case we haven't loaded the custom palette data, just abort here.
    if ( palMans[player - 1].find ( chara ) == palMans[player - 1].end() )
        return;

    palMans[player - 1][chara].apply ( palette, singlePaletteData );
}

} // namespace AsmHacks


// #include "PaletteManager.h"
// #include "StringUtils.h"
// #include "Constants.h"
// #include "Logger.h"

// #include <fstream>
// #include <sstream>

// using namespace std;


// // Palette number 36 is reserved for the current palette's state (including edits)
// #define RESERVED_PALETTES ( 1 )

// #define SWAP_R_AND_B(COLOR) ( ( COLOR & 0xFF ) << 16 ) | ( COLOR & 0xFF00 ) | ( ( COLOR & 0xFF0000 ) >> 16 )


// static inline char *getP1ColorTablePtr()
// {
//     const uint32_t *ptr0 = ( uint32_t * ) 0x74D808;

//     if ( ! *ptr0 )
//         return 0;

//     const uint32_t *ptr1 = ( uint32_t * ) ( *ptr0 + 0x1AC );

//     return ( char * ) *ptr1;
// }

// static inline uint32_t getP1Color ( uint32_t paletteNumber, uint32_t colorNumber )
// {
//     if ( ! DllPaletteManager::isReady() )
//         return 0;

//     const char *ptr = getP1ColorTablePtr();

//     const uint32_t color = ( 0xFFFFFF & * ( uint32_t * ) ( ptr + 4 + 1024 * paletteNumber + 4 * colorNumber ) );

//     // The internal color format is BGR; R and B are swapped
//     return SWAP_R_AND_B ( color );
// }

// static inline void setP1Color ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
// {
//     if ( ! DllPaletteManager::isReady() )
//         return;

//     const char *ptr = getP1ColorTablePtr();

//     // The internal color format is BGR; R and B are swapped
//     color = SWAP_R_AND_B ( color );

//     ( * ( uint32_t * ) ( ptr + 4 + 1024 * paletteNumber + 4 * colorNumber ) ) = ( 0xFFFFFF & color );
// }

// uint32_t DllPaletteManager::getHightlightColor ( uint32_t paletteNumber, uint32_t colorNumber ) const
// {
//     const uint32_t color = get ( paletteNumber, colorNumber );

//     const uint32_t r = ( color & 0xFF );
//     const uint32_t g = ( color & 0xFF00 ) >> 8;
//     const uint32_t b = ( color & 0xFF0000 ) >> 16;

//     const uint32_t absDivColor2 = 3 * 220 * 220;

//     if ( r * r + g * g + b * b > absDivColor2 )
//     {
//         return 0x111111; // Dark grey
//     }
//     else
//     {
//         return 0xFFFFFF; // White
//     }
// }

// bool DllPaletteManager::isReady()
// {
//     return getP1ColorTablePtr();
// }

// void DllPaletteManager::install()
// {
//     if ( installed )
//         return;

//     installed = true;

//     const char *ptr = getP1ColorTablePtr();

//     // Backup all the original palettes
//     for ( uint32_t i = 0; i < 36; ++i )
//     {
//         memcpy ( &backup[i][0], ( uint32_t * ) ( ptr + 4 + 1024 * i ), 1024 );
//     }

//     paletteNumber = *CC_P1_COLOR_SELECTOR_ADDR;

//     timer = 0;
//     *CC_P1_COLOR_SELECTOR_ADDR = 0;

//     // This also calls setColorNumber ( 0 )
//     setPaletteNumber ( paletteNumber );
// }

// void DllPaletteManager::uninstall()
// {
//     if ( ! installed )
//         return;

//     installed = false;

//     // Restore all the original palettes (with modifications)
//     for ( uint32_t i = 0; i < 36; ++i )
//     {
//         for ( uint32_t j = 0; j < 256; ++j )
//         {
//             setP1Color ( i, j, get ( i, j ) );
//         }
//     }
// }

// void DllPaletteManager::doneFlushing() const
// {
//     if ( ! installed )
//     {
//         *CC_P1_COLOR_SELECTOR_ADDR = paletteNumber;
//         return;
//     }

//     *CC_P1_COLOR_SELECTOR_ADDR = ( colorNumber % 36 );
// }

// void DllPaletteManager::frameStep()
// {
//     if ( ! installed )
//         return;

//     ++timer;

//     if ( timer % 24 )
//         return;

//     // This is the actual palette number the game reads from
//     const uint32_t actualPalette = colorNumber % ( 36 - RESERVED_PALETTES );

//     if ( *CC_P1_COLOR_SELECTOR_ADDR == actualPalette )
//         *CC_P1_COLOR_SELECTOR_ADDR = 0;
//     else
//         *CC_P1_COLOR_SELECTOR_ADDR = actualPalette;
// }

// uint32_t DllPaletteManager::getPaletteNumber() const
// {
//     return paletteNumber;
// }

// void DllPaletteManager::setPaletteNumber ( uint32_t paletteNumber )
// {
//     paletteNumber = paletteNumber % 36;

//     const char *ptr = getP1ColorTablePtr();

//     // Replace all the palettes with the specified one
//     for ( uint32_t i = 0; i < 36; ++i )
//     {
//         memcpy ( ( uint32_t * ) ( ptr + 4 + 1024 * i ), &backup[paletteNumber][0], 1024 );
//     }

//     this->paletteNumber = paletteNumber;
//     setColorNumber ( 0 );
// }

// uint32_t DllPaletteManager::getColorNumber() const
// {
//     return colorNumber;
// }

// void DllPaletteManager::setColorNumber ( uint32_t colorNumber )
// {
//     colorNumber = colorNumber % 256;

//     LOG ( "Color %02d - %03d", paletteNumber + 1, colorNumber + 1, 0 );

//     // Highlight all the colors in the range ( colorNumber - 18, colorNumber + 18 )
//     // by replacing colors in specific palettes.

//     uint32_t i = ( colorNumber + 256 - ( 18 - RESERVED_PALETTES ) ) % 256;
//     uint32_t j = ( ( 18 + colorNumber ) % 256 ) % ( 36 - RESERVED_PALETTES );

//     for ( ; i != ( colorNumber + 18 ) % 256; i = ( i + 1 ) % 256, j = ( j + 1 ) % ( 36 - RESERVED_PALETTES ) )
//     {
//         setP1Color ( j, i, getHightlightColor ( paletteNumber, colorNumber ) );

//         LOG ( "(%d, %d) -> %d", paletteNumber, i, j );
//     }

//     // This is the actual palette number we select
//     const uint32_t actualPalette = colorNumber % ( 36 - RESERVED_PALETTES );

//     this->colorNumber = colorNumber;
//     *CC_P1_COLOR_SELECTOR_ADDR = actualPalette;

//     timer = 0;
// }

// uint32_t DllPaletteManager::get ( uint32_t paletteNumber, uint32_t colorNumber ) const
// {
//     auto it = palettes.find ( paletteNumber );

//     if ( it != palettes.end() )
//     {
//         const auto jt = it->second.find ( colorNumber );

//         if ( jt != it->second.end() )
//             return jt->second;
//     }

//     // The backup colors have R and B swapped since they were copied directly from memory
//     return SWAP_R_AND_B ( backup[paletteNumber][colorNumber] );
// }

// void DllPaletteManager::set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
// {
//     // const auto it = backup.find ( paletteNumber );

//     // if ( it == backup.end() )
//     // {
//     //     backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
//     // }
//     // else
//     // {
//     //     const auto jt = it->second.find ( colorNumber );

//     //     if ( jt == it->second.end() )
//     //         backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
//     // }

//     // palettes[paletteNumber][colorNumber] = color;

//     // setP1Color ( paletteNumber, colorNumber, color );
// }

// void DllPaletteManager::clear ( uint32_t paletteNumber, uint32_t colorNumber )
// {
//     const auto it = palettes.find ( paletteNumber );

//     if ( it != palettes.end() )
//         it->second.erase ( colorNumber );
// }

// void DllPaletteManager::clear ( uint32_t paletteNumber )
// {
//     const auto it = palettes.find ( paletteNumber );

//     if ( it != palettes.end() )
//         it->second.clear();
// }

// void DllPaletteManager::clear()
// {
//     palettes.clear();
// }

// bool DllPaletteManager::save ( const string& file ) const
// {
//     ofstream fout ( file.c_str() );
//     bool good = fout.good();

//     // if ( good )
//     // {
//     //     fout << endl;

//     //     for ( auto it = palettes.cbegin(); it != palettes.cend(); ++it )
//     //     {
//     //         fout << format ( "// Color %02d", it->first ) << endl;

//     //         for ( const auto& kv : it->second )
//     //         {
//     //             const string line = format ( "color_%d_%d=%06X", it->first, kv.first, kv.second );

//     //             fout << line << endl;
//     //         }
//     //     }

//     //     good = fout.good();
//     // }

//     // fout.close();
//     return good;
// }

// bool DllPaletteManager::load ( const string& file )
// {
//     ifstream fin ( file.c_str() );
//     bool good = fin.good();

//     // if ( good )
//     // {
//     //     string line;

//     //     while ( getline ( fin, line ) )
//     //     {
//     //         vector<string> parts = split ( line, "=" );

//     //         if ( parts.size() != 2 )
//     //             continue;

//     //         vector<string> prefix = split ( parts[0], "_" );

//     //         if ( prefix.size() != 3 || prefix[0] != "color" )
//     //             continue;

//     //         const uint32_t paletteNumber = lexical_cast<uint32_t> ( prefix[1] );
//     //         const uint32_t colorNumber = lexical_cast<uint32_t> ( prefix[2] );

//     //         uint32_t color;

//     //         stringstream ss ( parts[1] );
//     //         ss >> hex >> color;

//     //         palettes[paletteNumber][colorNumber] = color;
//     //     }
//     // }

//     // fin.close();
//     return good;
// }
