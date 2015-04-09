#pragma once

#include <cstdint>
#include <map>
#include <array>
#include <string>


#define SWAP_R_AND_B(COLOR) \
    ( ( ( COLOR & 0xFF ) << 16 ) | ( COLOR & 0xFF00 ) | ( ( COLOR & 0xFF0000 ) >> 16 ) | ( COLOR & 0xFF000000 ) )


class PaletteManager
{
    std::map<uint32_t, std::map<uint32_t, uint32_t>> palettes;

    std::array<std::array<uint32_t, 256>, 36> originals;

public:

    static uint32_t computeHighlightColor ( uint32_t color );

    void cache ( const uint32_t **paletteData );
    void apply ( uint32_t **paletteData ) const;

    void cache ( const uint32_t *paletteData );
    void apply ( uint32_t *paletteData ) const;

    uint32_t getOriginal ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    uint32_t get ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    void set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color );

    void clear ( uint32_t paletteNumber, uint32_t colorNumber );
    void clear ( uint32_t paletteNumber );
    void clear();

    bool empty() const;

    bool save ( const std::string& file ) const;
    bool load ( const std::string& file );
};
