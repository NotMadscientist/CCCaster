#pragma once

#include "KeyValueStore.h"

#include <map>


class DllPaletteManager
{
    std::map<uint32_t, std::map<uint32_t, uint32_t>> palettes, backup;

public:

    bool isReady() const;

    uint32_t get ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    void set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color );

    void clear ( uint32_t paletteNumber, uint32_t colorNumber );
    void clear ( uint32_t paletteNumber );
    void clear();

    bool save ( const std::string& file ) const;
    bool load ( const std::string& file );
};
