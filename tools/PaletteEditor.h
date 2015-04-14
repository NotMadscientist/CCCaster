#pragma once

#include "PaletteManager.h"

#include "mbaacc_framedisplay.h"

#include <cstdint>
#include <string>
#include <unordered_map>


class PaletteEditor
{
    std::string palettesFolder;

    MBAACC_FrameDisplay frameDisp;

    std::unordered_map<uint32_t, PaletteManager> palMans;

    int paletteNumber = 0, colorNumber = 0;

    uint32_t charaNumToIndex[256], charaIndexToNum[256];


    void loadCurrentChara();

    void saveCurrentChara();

    void applyColor ( uint32_t color );

    int getCharaIndex();

public:

    uint32_t ticker = 0;


    bool init ( const std::string& palettesFolder, const std::string& dataFile );

    void free();


    void save ( int palettteNumber = -1 );


    uint32_t getOriginalColor();

    std::string getOriginalColorHex();


    uint32_t getCurrentColor();

    std::string getCurrentColorHex();

    void setCurrentColor ( uint32_t color );

    void setCurrentColor ( std::string colorHex );

    void clearCurrentColor();


    void highlightCurrentColor ( bool highlight );


    int getPaletteNumber() const;

    void setPaletteNumber ( int paletteNumber );


    int getColorNumber() const;

    void setColorNumber ( int colorNumber );


    int getChara();

    int getCharaCount();

    const char *getCharaName();

    const char *getCharaName ( int chara );

    void setChara ( int chara );


    int getSpriteNumber();

    void setSpriteNumber ( int spriteNumber );


    int getSpriteFrame();

    void setSpriteFrame ( int spriteFrame );


    void nextSpriteSubFrame();


    void renderSprite();


    static bool isValidColor ( const std::string& str );
};
