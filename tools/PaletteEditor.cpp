#include "PaletteEditor.h"
#include "CharacterSelect.h"
#include "StringUtils.h"

using namespace std;


void PaletteEditor::loadCurrentChara()
{
    if ( palMans.find ( getChara() ) != palMans.end() )
        return;

    palMans[getChara()].cache ( static_cast<const MBAACC_FrameDisplay&> ( frameDisp ).get_palette_data() );
    palMans[getChara()].load ( palettesFolder, getCharaName() );
}

int PaletteEditor::getCharaIndex()
{
    return charaNumToIndex[frameDisp.get_character() / 3];
}

bool PaletteEditor::init ( const std::string& palettesFolder, const std::string& dataFile )
{
    this->palettesFolder = palettesFolder;

    if ( !frameDisp.init ( dataFile.c_str() ) )
        return false;

    for ( int i = 0; i < frameDisp.get_character_count() / 3; ++i )
    {
        const int index = frameDisp.get_character_index ( i * 3 );
        charaNumToIndex[i] = index;
        charaIndexToNum[index] = i;
    }

    return true;
}

void PaletteEditor::free()
{
    frameDisp.free();
}

uint32_t PaletteEditor::getOriginalColor()
{
    loadCurrentChara();
    return 0xFFFFFF & palMans[getChara()].getOriginal ( paletteNumber, colorNumber );
}

string PaletteEditor::getOriginalColorHex()
{
    return format ( "%06X", getOriginalColor() );
}

uint32_t PaletteEditor::getCurrentColor()
{
    loadCurrentChara();
    return 0xFFFFFF & palMans[getChara()].get ( paletteNumber, colorNumber );
}

string PaletteEditor::getCurrentColorHex()
{
    return format ( "%06X", getCurrentColor() );
}

void PaletteEditor::setCurrentColor ( uint32_t color )
{
}

void PaletteEditor::setCurrentColor ( const string& colorHex )
{
}

int PaletteEditor::getPaletteNumber() const
{
    return paletteNumber;
}

void PaletteEditor::setPaletteNumber ( int paletteNumber )
{
    frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
    this->paletteNumber = frameDisp.get_palette();
}

int PaletteEditor::getColorNumber() const
{
    return colorNumber;
}

void PaletteEditor::setColorNumber ( int colorNumber )
{
    PaletteEditor::colorNumber = colorNumber;
}

int PaletteEditor::getChara()
{
    return frameDisp.get_character() / 3;
}

int PaletteEditor::getCharaCount()
{
    return frameDisp.get_character_count() / 3;
}

const char *PaletteEditor::getCharaName()
{
    return getShortCharaName ( getCharaIndex() );
}

const char *PaletteEditor::getCharaName ( int chara )
{
    return getShortCharaName ( frameDisp.get_character_index ( chara * 3 ) );
}

void PaletteEditor::setChara ( int chara )
{
    chara *= 3;
    frameDisp.command ( COMMAND_CHARACTER_SET, &chara );
}

int PaletteEditor::getSpriteNumber()
{
    return frameDisp.get_sequence();
}

void PaletteEditor::setSpriteNumber ( int spriteNumber )
{
}

void PaletteEditor::nextSpriteFrame()
{
    frameDisp.command ( COMMAND_SUBFRAME_NEXT, 0 );
}

void PaletteEditor::renderSprite()
{
    static const RenderProperties renderProps = { 1, 0, 0, 0, 0, 0 };
    frameDisp.render ( &renderProps );
}
