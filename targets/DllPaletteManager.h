#pragma once

#include "KeyValueStore.h"

#include <map>
#include <array>


class DllPaletteManager
{
    std::map<uint32_t, std::map<uint32_t, uint32_t>> palettes;

    std::array<std::array<uint32_t, 256>, 36> backup;

    uint32_t paletteNumber = 0, colorNumber = 0;

    uint32_t timer = 0;

    bool installed = false;


    uint32_t getHightlightColor ( uint32_t paletteNumber, uint32_t colorNumber ) const;

public:

    static bool isReady();

    void install();
    void uninstall();

    void doneFlushing() const;

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
