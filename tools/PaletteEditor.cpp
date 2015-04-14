#include "PaletteEditor.h"
#include "CharacterSelect.h"
#include "StringUtils.h"
#include "Algorithms.h"

#include <direct.h>

#include <cctype>

using namespace std;


void PaletteEditor::loadCurrentChara()
{
    if ( palMans.find ( getChara() ) == palMans.end() )
    {
        palMans[getChara()].cache ( static_cast<const MBAACC_FrameDisplay&> ( frameDisp ).get_palette_data() );
        palMans[getChara()].load ( palettesFolder, getCharaName() );
    }

    palMans[getChara()].apply ( frameDisp.get_palette_data() );
}

void PaletteEditor::saveCurrentChara()
{
    if ( palMans.find ( getChara() ) == palMans.end() )
        return;

    _mkdir ( palettesFolder.c_str() );
    palMans[getChara()].save ( palettesFolder, getCharaName() );
}

void PaletteEditor::applyColor ( uint32_t color )
{
    uint32_t& currColor = frameDisp.get_palette_data() [paletteNumber][colorNumber];
    const uint32_t newColor = ( currColor & 0xFF000000 ) | ( SWAP_R_AND_B ( color ) & 0xFFFFFF );

    if ( currColor == newColor )
        return;

    currColor = newColor;
    frameDisp.flush_texture();
}

int PaletteEditor::getCharaIndex()
{
    return charaNumToIndex[frameDisp.get_character() / 3]; // Because FrameDisplay has each moon
}

bool PaletteEditor::init ( const std::string& palettesFolder, const std::string& dataFile )
{
    this->palettesFolder = palettesFolder;

    if ( !frameDisp.init ( dataFile.c_str() ) )
        return false;

    // Because FrameDisplay has each moon
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

void PaletteEditor::save ( int paletteNumber )
{
    if ( paletteNumber >= 0 && paletteNumber != this->paletteNumber )
    {
        for ( int i = 0; i < 256; ++i )
        {
            palMans[getChara()].set ( paletteNumber, i, palMans[getChara()].get ( this->paletteNumber, i ) );
        }

        palMans[getChara()].apply ( frameDisp.get_palette_data() );
        frameDisp.flush_texture();
    }

    saveCurrentChara();
}

uint32_t PaletteEditor::getOriginalColor()
{
    loadCurrentChara();
    return palMans[getChara()].getOriginal ( paletteNumber, colorNumber );
}

string PaletteEditor::getOriginalColorHex()
{
    return format ( "%06X", getOriginalColor() );
}

uint32_t PaletteEditor::getCurrentColor()
{
    loadCurrentChara();
    return palMans[getChara()].get ( paletteNumber, colorNumber );
}

string PaletteEditor::getCurrentColorHex()
{
    return format ( "%06X", getCurrentColor() );
}

void PaletteEditor::setCurrentColor ( uint32_t color )
{
    loadCurrentChara();
    palMans[getChara()].set ( paletteNumber, colorNumber, color );

    applyColor ( color );

    ticker = 0;
}

void PaletteEditor::setCurrentColor ( string colorHex )
{
    colorHex = colorHex.substr ( 0, 6 );

    if ( ! isValidColor ( colorHex ) )
        return;

    if ( colorHex.size() < 6 )
        colorHex += string ( 6 - colorHex.size(), colorHex.back() );

    setCurrentColor ( parseHex<uint32_t> ( colorHex ) );
}

void PaletteEditor::clearCurrentColor()
{
    loadCurrentChara();
    palMans[getChara()].clear ( paletteNumber, colorNumber );

    applyColor ( palMans[getChara()].get ( paletteNumber, colorNumber ) );
}

void PaletteEditor::highlightCurrentColor ( bool highlight )
{
    uint32_t color = palMans[getChara()].get ( paletteNumber, colorNumber );

    if ( highlight )
        color = PaletteManager::computeHighlightColor ( color );

    applyColor ( color );
}

int PaletteEditor::getPaletteNumber() const
{
    return paletteNumber;
}

void PaletteEditor::setPaletteNumber ( int paletteNumber )
{
    if ( paletteNumber < 0 )
        paletteNumber = 35;
    else if ( paletteNumber > 35 )
        paletteNumber = 0;

    frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );
    this->paletteNumber = frameDisp.get_palette();
    loadCurrentChara();
}

int PaletteEditor::getColorNumber() const
{
    return colorNumber;
}

void PaletteEditor::setColorNumber ( int colorNumber )
{
    if ( colorNumber < 0 )
        this->colorNumber = 255;
    else if ( colorNumber > 255 )
        this->colorNumber = 0;
    else
        this->colorNumber = colorNumber;
    ticker = 0;
}

int PaletteEditor::getChara()
{
    return frameDisp.get_character() / 3; // Because FrameDisplay has each moon
}

int PaletteEditor::getCharaCount()
{
    return frameDisp.get_character_count() / 3; // Because FrameDisplay has each moon
}

const char *PaletteEditor::getCharaName()
{
    return getShortCharaName ( getCharaIndex() );
}

const char *PaletteEditor::getCharaName ( int chara )
{
    return getShortCharaName ( frameDisp.get_character_index ( chara * 3 ) ); // Because FrameDisplay has each moon
}

void PaletteEditor::setChara ( int chara )
{
    colorNumber = paletteNumber = 0;
    frameDisp.command ( COMMAND_PALETTE_SET, &paletteNumber );

    chara *= 3; // Because FrameDisplay has each moon
    frameDisp.command ( COMMAND_CHARACTER_SET, &chara );

    loadCurrentChara();
}

int PaletteEditor::getSpriteNumber()
{
    return frameDisp.get_sequence();
}

void PaletteEditor::setSpriteNumber ( int spriteNumber )
{
    frameDisp.command ( COMMAND_SEQUENCE_SET, &spriteNumber );
}

int PaletteEditor::getSpriteFrame()
{
    return frameDisp.get_subframe();
}

void PaletteEditor::setSpriteFrame ( int spriteFrame )
{
    if ( spriteFrame == frameDisp.get_subframe() - 1 )
    {
        frameDisp.command ( COMMAND_FRAME_PREV, 0 );
        return;
    }

    if ( spriteFrame == frameDisp.get_subframe() + 1 )
    {
        frameDisp.command ( COMMAND_FRAME_NEXT, 0 );
        return;
    }

    frameDisp.command ( COMMAND_SUBFRAME_SET, &spriteFrame );
}

void PaletteEditor::nextSpriteSubFrame()
{
    frameDisp.command ( COMMAND_SUBFRAME_NEXT, 0 );
}

void PaletteEditor::renderSprite()
{
    static const RenderProperties renderProps = { 1, 0, 0, 0, 0, 0 };
    frameDisp.render ( &renderProps );
}

bool PaletteEditor::isValidColor ( const string& str )
{
    if ( str.empty() )
        return false;

    for ( char c : str )
    {
        c = toupper ( c );

        if ( ! ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) ) )
        {
            return false;
        }
    }

    return true;
}
