#pragma once

#ifndef DISABLE_SERIALIZATION
#include "Protocol.hpp"
#include <cereal/types/map.hpp>
#endif

#include <cstdint>
#include <map>
#include <array>
#include <string>


#define COLOR_RGB(R, G, B) \
    ( 0xFFFFFF & ( ( ( 0xFF & ( R ) ) << 16 ) | ( ( 0xFF & ( G ) ) << 8 ) | ( 0xFF & ( B ) ) ) )

#define SWAP_R_AND_B(COLOR) \
    ( ( ( COLOR & 0xFF ) << 16 ) | ( COLOR & 0xFF00 ) | ( ( COLOR & 0xFF0000 ) >> 16 ) | ( COLOR & 0xFF000000 ) )


#ifndef DISABLE_SERIALIZATION
class PaletteManager : public SerializableSequence
#else
class PaletteManager
#endif
{
    std::map<uint32_t, std::map<uint32_t, uint32_t>> palettes;

    std::array<std::array<uint32_t, 256>, 36> originals;

    void optimize();

public:

    static uint32_t computeHighlightColor ( uint32_t color );

    void cache ( const uint32_t **allPaletteData );
    void apply ( uint32_t **allPaletteData ) const;

    void cache ( const uint32_t *allPaletteData );
    void apply ( uint32_t *allPaletteData ) const;
    void apply ( uint32_t paletteNumber, uint32_t *singlePaletteData ) const;

    uint32_t getOriginal ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    uint32_t get ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    void set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color );

    void clear ( uint32_t paletteNumber, uint32_t colorNumber );
    void clear ( uint32_t paletteNumber );
    void clear();

    bool empty() const;

    bool save ( const std::string& folder, const std::string& charaName );
    bool load ( const std::string& folder, const std::string& charaName );

#ifndef DISABLE_SERIALIZATION
    PROTOCOL_MESSAGE_BOILERPLATE ( PaletteManager, palettes )
#endif
};
