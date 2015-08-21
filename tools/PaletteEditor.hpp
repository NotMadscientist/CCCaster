#pragma once

#include "PaletteManager.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>


struct MBAACC_FrameDisplay;

class PaletteEditor
{
public:

    uint32_t ticker = 0;

    PaletteEditor();

    bool init ( const std::string& palettesFolder, const std::string& dataFile );

    void save ( int palettteNumber = -1 );

    uint32_t getOriginalColor();
    std::string getOriginalColorHex();

    uint32_t getCurrentColor();
    std::string getCurrentColorHex();

    void setCurrentColor ( uint32_t color );
    void setCurrentColor ( std::string colorHex );

    void clearCurrentColor();

    void highlightCurrentColor ( bool highlight );

    int getPaletteNumber() const { return _paletteNumber; }
    void setPaletteNumber ( int paletteNumber );

    int getColorNumber() const { return _colorNumber; }
    void setColorNumber ( int colorNumber );

    int getChara();
    void setChara ( int chara );

    int getCharaCount();
    const char *getCharaName();
    const char *getCharaName ( int chara );

    int getSpriteNumber();
    void setSpriteNumber ( int spriteNumber );

    int getSpriteFrame();
    void setSpriteFrame ( int spriteFrame );

    void nextSpriteSubFrame();

    void renderSprite();

    static bool isValidColor ( const std::string& str );

private:

    std::string _palettesFolder;

    std::shared_ptr<MBAACC_FrameDisplay> _frameDisp;

    std::unordered_map<uint32_t, PaletteManager> _palMans;

    int _paletteNumber = 0, _colorNumber = 0;

    uint32_t _charaNumToIndex[256], _charaIndexToNum[256];

    void loadCurrentChara();
    void saveCurrentChara();

    void applyColor ( uint32_t color );

    int getCharaIndex();
};
