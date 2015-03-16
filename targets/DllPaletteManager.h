#pragma once

#include "KeyValueStore.h"

#include <map>
#include <array>


class DllPaletteManager
{
    // std::map<uint32_t, std::map<uint32_t, uint32_t>> palettes, backup;

    std::array<std::array<uint32_t, 256>, 36> backup;

    uint32_t paletteNumber = 0, colorNumber = 0;

    uint32_t timer = 0;

public:

    static bool isReady();

    void install();
    void sync();

    void frameStep();

    uint32_t getPaletteNumber() const;
    void setPaletteNumber ( uint32_t paletteNumber );

    uint32_t getColorNumber() const;
    void setColorNumber ( uint32_t colorNumber );

    uint32_t get ( uint32_t paletteNumber, uint32_t colorNumber ) const;
    void set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color );

    void clear ( uint32_t paletteNumber, uint32_t colorNumber );
    void clear ( uint32_t paletteNumber );
    void clear();

    bool save ( const std::string& file ) const;
    bool load ( const std::string& file );
};
