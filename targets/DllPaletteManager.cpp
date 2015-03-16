#include "DllPaletteManager.h"
#include "StringUtils.h"
#include "Constants.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

using namespace std;


#define HIGHLIGHT_COLOR ( 0x00FF00 )


static inline char *getP1ColorTablePtr()
{
    const uint32_t *ptr0 = ( uint32_t * ) 0x74D808;

    if ( ! *ptr0 )
        return 0;

    const uint32_t *ptr1 = ( uint32_t * ) ( *ptr0 + 0x1AC );

    return ( char * ) *ptr1;
}

static inline uint32_t getP1Color ( uint32_t paletteNumber, uint32_t colorNumber )
{
    if ( ! DllPaletteManager::isReady() )
        return 0;

    const char *ptr = getP1ColorTablePtr();

    uint32_t color = ( 0xFFFFFF & * ( uint32_t * ) ( ptr + 4 + 1024 * paletteNumber + 4 * colorNumber ) );

    // The internal color format is BGR; R and B are swapped
    return ( ( color & 0xFF ) << 16 ) | ( color & 0xFF00 ) | ( ( color & 0xFF0000 ) >> 16 );
}

static inline void setP1Color ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    if ( ! DllPaletteManager::isReady() )
        return;

    const char *ptr = getP1ColorTablePtr();

    // The internal color format is BGR; R and B are swapped
    color = ( ( color & 0xFF ) << 16 ) | ( color & 0xFF00 ) | ( ( color & 0xFF0000 ) >> 16 );

    ( * ( uint32_t * ) ( ptr + 4 + 1024 * paletteNumber + 4 * colorNumber ) ) = ( 0xFFFFFF & color );
}

bool DllPaletteManager::isReady()
{
    return getP1ColorTablePtr();
}

void DllPaletteManager::install()
{
    const char *ptr = getP1ColorTablePtr();

    // Backup all the original palettes / colors
    for ( uint32_t i = 0; i < 36; ++i )
    {
        memcpy ( &backup[i][0], ( uint32_t * ) ( ptr + 4 + 1024 * i ), 1024 );
    }

    paletteNumber = *CC_P1_COLOR_SELECTOR_ADDR;

    timer = 0;
    *CC_P1_COLOR_SELECTOR_ADDR = 0;

    setPaletteNumber ( paletteNumber );
}

void DllPaletteManager::sync()
{
    *CC_P1_COLOR_SELECTOR_ADDR = ( colorNumber % 36 );
}

// Palette number 35 is reserved for the original palette
// Palette number 36 is reserved for the current color being edited
static const uint32_t ReservedPalettes = 2;

void DllPaletteManager::frameStep()
{
    ++timer;

    if ( timer % 24 )
        return;

    const uint32_t actualPalette = colorNumber % ( 36 - ReservedPalettes );

    if ( *CC_P1_COLOR_SELECTOR_ADDR == actualPalette )
        *CC_P1_COLOR_SELECTOR_ADDR = 35;
    else
        *CC_P1_COLOR_SELECTOR_ADDR = actualPalette;
}

uint32_t DllPaletteManager::getPaletteNumber() const
{
    return paletteNumber;
}

void DllPaletteManager::setPaletteNumber ( uint32_t paletteNumber )
{
    paletteNumber = paletteNumber % 36;

    const char *ptr = getP1ColorTablePtr();

    // Replace all the palettes with the specified one
    for ( uint32_t i = 0; i < 36; ++i )
    {
        memcpy ( ( uint32_t * ) ( ptr + 4 + 1024 * i ), &backup[paletteNumber][0], 1024 );
    }

    this->paletteNumber = paletteNumber;
    setColorNumber ( 0 );
}

uint32_t DllPaletteManager::getColorNumber() const
{
    return colorNumber;
}

void DllPaletteManager::setColorNumber ( uint32_t colorNumber )
{
    colorNumber = colorNumber % 256;

    const uint32_t actualPalette = colorNumber % ( 36 - ReservedPalettes );

    // Highlight all the colors in the range ( colorNumber - 18, colorNumber + 17 )
    // by replacing colors in specific palettes.

    for ( uint32_t i = 0; i < 18; ++i )
    {
        setP1Color ( ( actualPalette + i ) % ( 36 - ReservedPalettes ),
                     ( colorNumber + i ) % 256,
                     HIGHLIGHT_COLOR );
    }

    for ( uint32_t i = 1; i < 19 - ReservedPalettes; ++i )
    {
        setP1Color ( ( actualPalette + uint32_t ( 36 - ReservedPalettes - i ) ) % ( 36 - ReservedPalettes ),
                     ( colorNumber + uint32_t ( 256 - i ) ) % 256,
                     HIGHLIGHT_COLOR );
    }

    this->colorNumber = colorNumber;
    *CC_P1_COLOR_SELECTOR_ADDR = actualPalette;

    timer = 0;

    LOG ( "Color %02d - %03d", this->paletteNumber + 1, this->colorNumber + 1, 0 );
}

uint32_t DllPaletteManager::get ( uint32_t paletteNumber, uint32_t colorNumber ) const
{
    // auto it = palettes.find ( paletteNumber );

    // if ( it != palettes.end() )
    // {
    //     const auto jt = it->second.find ( colorNumber );

    //     if ( jt != it->second.end() )
    //         return jt->second;
    // }

    // it = backup.find ( paletteNumber );

    // if ( it != backup.end() )
    // {
    //     const auto jt = it->second.find ( colorNumber );

    //     if ( jt != it->second.end() )
    //         return jt->second;
    // }

    // return getP1Color ( paletteNumber, colorNumber );

    return 0;
}

void DllPaletteManager::set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    // const auto it = backup.find ( paletteNumber );

    // if ( it == backup.end() )
    // {
    //     backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
    // }
    // else
    // {
    //     const auto jt = it->second.find ( colorNumber );

    //     if ( jt == it->second.end() )
    //         backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
    // }

    // palettes[paletteNumber][colorNumber] = color;

    // setP1Color ( paletteNumber, colorNumber, color );
}

void DllPaletteManager::clear ( uint32_t paletteNumber, uint32_t colorNumber )
{
    // const auto it = palettes.find ( paletteNumber );

    // if ( it != palettes.end() )
    //     it->second.erase ( colorNumber );
}

void DllPaletteManager::clear ( uint32_t paletteNumber )
{
    // const auto it = palettes.find ( paletteNumber );

    // if ( it != palettes.end() )
    //     it->second.clear();
}

void DllPaletteManager::clear()
{
    // palettes.clear();
}

bool DllPaletteManager::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    // if ( good )
    // {
    //     fout << endl;

    //     for ( auto it = palettes.cbegin(); it != palettes.cend(); ++it )
    //     {
    //         fout << format ( "// Color %02d", it->first ) << endl;

    //         for ( const auto& kv : it->second )
    //         {
    //             const string line = format ( "color_%d_%d=%06X", it->first, kv.first, kv.second );

    //             fout << line << endl;
    //         }
    //     }

    //     good = fout.good();
    // }

    // fout.close();
    return good;
}

bool DllPaletteManager::load ( const string& file )
{
    ifstream fin ( file.c_str() );
    bool good = fin.good();

    // if ( good )
    // {
    //     string line;

    //     while ( getline ( fin, line ) )
    //     {
    //         vector<string> parts = split ( line, "=" );

    //         if ( parts.size() != 2 )
    //             continue;

    //         vector<string> prefix = split ( parts[0], "_" );

    //         if ( prefix.size() != 3 || prefix[0] != "color" )
    //             continue;

    //         const uint32_t paletteNumber = lexical_cast<uint32_t> ( prefix[1] );
    //         const uint32_t colorNumber = lexical_cast<uint32_t> ( prefix[2] );

    //         uint32_t color;

    //         stringstream ss ( parts[1] );
    //         ss >> hex >> color;

    //         palettes[paletteNumber][colorNumber] = color;
    //     }
    // }

    // fin.close();
    return good;
}
