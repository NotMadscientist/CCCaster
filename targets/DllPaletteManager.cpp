#include "DllPaletteManager.h"
#include "StringUtils.h"

#include <fstream>
#include <sstream>

using namespace std;


static inline uint32_t getP1Color ( uint32_t paletteNumber, uint32_t colorNumber )
{
    const uint32_t *ptr0 = ( uint32_t * ) 0x74D808;

    if ( ! *ptr0 )
        return 0;

    const uint32_t *ptr1 = ( uint32_t * ) ( *ptr0 + 0x1AC );

    if ( ! *ptr1 )
        return 0;

    uint32_t color = ( 0xFFFFFF & * ( uint32_t * ) ( *ptr1 + 4 + 1024 * paletteNumber + 4 * colorNumber ) );

    // The internal color format is BGR; R and B are swapped
    return ( ( color & 0xFF ) << 16 ) | ( color & 0xFF00 ) | ( ( color & 0xFF0000 ) >> 16 );
}

static inline void setP1Color ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    const uint32_t *ptr0 = ( uint32_t * ) 0x74D808;

    if ( ! *ptr0 )
        return;

    const uint32_t *ptr1 = ( uint32_t * ) ( *ptr0 + 0x1AC );

    if ( ! *ptr1 )
        return;

    // The internal color format is BGR; R and B are swapped
    color = ( ( color & 0xFF ) << 16 ) | ( color & 0xFF00 ) | ( ( color & 0xFF0000 ) >> 16 );

    ( * ( uint32_t * ) ( *ptr1 + 4 + 1024 * paletteNumber + 4 * colorNumber ) ) = ( 0xFFFFFF & color );
}

bool DllPaletteManager::isReady() const
{
    const uint32_t *ptr0 = ( uint32_t * ) 0x74D808;

    if ( ! *ptr0 )
        return false;

    const uint32_t *ptr1 = ( uint32_t * ) ( *ptr0 + 0x1AC );

    if ( ! *ptr1 )
        return false;

    return true;
}

uint32_t DllPaletteManager::get ( uint32_t paletteNumber, uint32_t colorNumber ) const
{
    auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
    {
        const auto jt = it->second.find ( colorNumber );

        if ( jt != it->second.end() )
            return jt->second;
    }

    it = backup.find ( paletteNumber );

    if ( it != backup.end() )
    {
        const auto jt = it->second.find ( colorNumber );

        if ( jt != it->second.end() )
            return jt->second;
    }

    return getP1Color ( paletteNumber, colorNumber );
}

void DllPaletteManager::set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    const auto it = backup.find ( paletteNumber );

    if ( it == backup.end() )
    {
        backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
    }
    else
    {
        const auto jt = it->second.find ( colorNumber );

        if ( jt == it->second.end() )
            backup[paletteNumber][colorNumber] = getP1Color ( paletteNumber, colorNumber );
    }

    palettes[paletteNumber][colorNumber] = color;

    setP1Color ( paletteNumber, colorNumber, color );
}

void DllPaletteManager::clear ( uint32_t paletteNumber, uint32_t colorNumber )
{
    const auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
        it->second.erase ( colorNumber );
}

void DllPaletteManager::clear ( uint32_t paletteNumber )
{
    const auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
        it->second.clear();
}

void DllPaletteManager::clear()
{
    palettes.clear();
}

bool DllPaletteManager::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    if ( good )
    {
        fout << endl;

        for ( auto it = palettes.cbegin(); it != palettes.cend(); ++it )
        {
            fout << format ( "// Color %02d", it->first ) << endl;

            for ( const auto& kv : it->second )
            {
                const string line = format ( "color_%d_%d=%06X", it->first, kv.first, kv.second );

                fout << line << endl;
            }
        }

        good = fout.good();
    }

    fout.close();
    return good;
}

bool DllPaletteManager::load ( const string& file )
{
    ifstream fin ( file.c_str() );
    bool good = fin.good();

    if ( good )
    {
        string line;

        while ( getline ( fin, line ) )
        {
            vector<string> parts = split ( line, "=" );

            if ( parts.size() != 2 )
                continue;

            vector<string> prefix = split ( parts[0], "_" );

            if ( prefix.size() != 3 || prefix[0] != "color" )
                continue;

            const uint32_t paletteNumber = lexical_cast<uint32_t> ( prefix[1] );
            const uint32_t colorNumber = lexical_cast<uint32_t> ( prefix[2] );

            uint32_t color;

            stringstream ss ( parts[1] );
            ss >> hex >> color;

            palettes[paletteNumber][colorNumber] = color;
        }
    }

    fin.close();
    return good;
}
