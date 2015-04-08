#include "PaletteManager.h"
#include "StringUtils.h"

#include <fstream>
#include <sstream>

using namespace std;


uint32_t PaletteManager::computeHighlightColor ( uint32_t color )
{
    const uint32_t r = ( color & 0xFF );
    const uint32_t g = ( color & 0xFF00 ) >> 8;
    const uint32_t b = ( color & 0xFF0000 ) >> 16;

    const uint32_t absDivColor2 = 3 * 220 * 220;

    if ( r * r + g * g + b * b > absDivColor2 )
    {
        return ( color & 0xFF000000 ) | 0x111111; // Dark grey
    }
    else
    {
        return ( color & 0xFF000000 ) | 0xFFFFFF; // White
    }
}

void PaletteManager::init ( uint32_t **originalPalettes )
{
    for ( uint32_t i = 0; i < originals.size(); ++i )
    {
        for ( uint32_t j = 0; j < originals[i].size(); ++j )
        {
            originals[i][j] = SWAP_R_AND_B ( originalPalettes[i][j] );
        }
    }
}

uint32_t PaletteManager::get ( uint32_t paletteNumber, uint32_t colorNumber ) const
{
    auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
    {
        const auto jt = it->second.find ( colorNumber );

        if ( jt != it->second.end() )
            return jt->second;
    }

    return originals[paletteNumber][colorNumber];
}

void PaletteManager::set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    palettes[paletteNumber][colorNumber] = color;
}

void PaletteManager::clear ( uint32_t paletteNumber, uint32_t colorNumber )
{
    const auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
        it->second.erase ( colorNumber );
}

void PaletteManager::clear ( uint32_t paletteNumber )
{
    const auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
        it->second.clear();
}

void PaletteManager::clear()
{
    palettes.clear();
}

bool PaletteManager::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    if ( good )
    {
        fout << endl;

        for ( auto it = palettes.cbegin(); it != palettes.cend(); ++it )
        {
            fout << format ( "\n======== Color %02d start ========\n", it->first ) << endl;

            for ( const auto& kv : it->second )
            {
                const string line = format ( "color_%02d_%03d=#%06X", it->first, kv.first, kv.second );

                fout << line << endl;
            }

            fout << format ( "\n======== Color %02d end ========\n", it->first ) << endl;
        }

        good = fout.good();
    }

    fout.close();
    return good;
}

bool PaletteManager::load ( const string& file )
{
    ifstream fin ( file.c_str() );
    bool good = fin.good();

    if ( good )
    {
        string line;

        while ( getline ( fin, line ) )
        {
            vector<string> parts = split ( line, "=" );

            if ( parts.size() != 2 || parts[0].empty() || parts[1].empty() || parts[1][0] != '#' )
                continue;

            vector<string> prefix = split ( parts[0], "_" );

            if ( prefix.size() != 3 || prefix[0] != "color" || prefix[1].empty() || prefix[2].empty() )
                continue;

            const uint32_t paletteNumber = lexical_cast<uint32_t> ( prefix[1] );
            const uint32_t colorNumber = lexical_cast<uint32_t> ( prefix[2] );

            uint32_t color;

            stringstream ss ( parts[1].substr ( 1 ) );
            ss >> hex >> color;

            palettes[paletteNumber][colorNumber] = color;
        }
    }

    fin.close();
    return good;
}
