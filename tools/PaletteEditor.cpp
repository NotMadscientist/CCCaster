#include "PaletteEditor.hpp"
#include "CharacterSelect.hpp"
#include "StringUtils.hpp"
#include "Algorithms.hpp"

#include "mbaacc_framedisplay.h"

#include <direct.h>

#include <cctype>

using namespace std;


PaletteEditor::PaletteEditor() : _frameDisp ( new MBAACC_FrameDisplay() )
{
}

bool PaletteEditor::init ( const string& _palettesFolder, const string& dataFile )
{
    this->_palettesFolder = _palettesFolder;

    if ( ! _frameDisp->init ( dataFile.c_str() ) )
        return false;

    // Because FrameDisplay has each moon
    for ( int i = 0; i < _frameDisp->get_character_count() / 3; ++i )
    {
        const int index = _frameDisp->get_character_index ( i * 3 );
        _charaNumToIndex[i] = index;
        _charaIndexToNum[index] = i;
    }

    return true;
}

void PaletteEditor::save ( int paletteNumber )
{
    if ( paletteNumber >= 0 && paletteNumber != _paletteNumber )
    {
        for ( int i = 0; i < 256; ++i )
        {
            _palMans[getChara()].set ( paletteNumber, i, _palMans[getChara()].get ( _paletteNumber, i ) );
        }

        _palMans[getChara()].apply ( _frameDisp->get_palette_data() );
        _frameDisp->flush_texture();
    }

    saveCurrentChara();
}

uint32_t PaletteEditor::getOriginalColor()
{
    loadCurrentChara();
    return _palMans[getChara()].getOriginal ( _paletteNumber, _colorNumber );
}

string PaletteEditor::getOriginalColorHex()
{
    return format ( "%06X", getOriginalColor() );
}

uint32_t PaletteEditor::getCurrentColor()
{
    loadCurrentChara();
    return _palMans[getChara()].get ( _paletteNumber, _colorNumber );
}

string PaletteEditor::getCurrentColorHex()
{
    return format ( "%06X", getCurrentColor() );
}

void PaletteEditor::setCurrentColor ( uint32_t color )
{
    loadCurrentChara();
    _palMans[getChara()].set ( _paletteNumber, _colorNumber, color );

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
    _palMans[getChara()].clear ( _paletteNumber, _colorNumber );

    applyColor ( _palMans[getChara()].get ( _paletteNumber, _colorNumber ) );
}

void PaletteEditor::highlightCurrentColor ( bool highlight )
{
    uint32_t color = _palMans[getChara()].get ( _paletteNumber, _colorNumber );

    if ( highlight )
        color = PaletteManager::computeHighlightColor ( color );

    applyColor ( color );
}

void PaletteEditor::setPaletteNumber ( int paletteNumber )
{
    if ( paletteNumber < 0 )
        paletteNumber = 35;
    else if ( paletteNumber > 35 )
        paletteNumber = 0;

    _frameDisp->command ( COMMAND_PALETTE_SET, &paletteNumber );
    _paletteNumber = _frameDisp->get_palette();
    loadCurrentChara();
}

void PaletteEditor::setColorNumber ( int colorNumber )
{
    if ( colorNumber < 0 )
        _colorNumber = 255;
    else if ( colorNumber > 255 )
        _colorNumber = 0;
    else
        _colorNumber = colorNumber;
    ticker = 0;
}

int PaletteEditor::getChara()
{
    return _frameDisp->get_character() / 3; // Because FrameDisplay has each moon
}

void PaletteEditor::setChara ( int chara )
{
    _colorNumber = _paletteNumber = 0;
    _frameDisp->command ( COMMAND_PALETTE_SET, &_paletteNumber );

    chara *= 3; // Because FrameDisplay has each moon
    _frameDisp->command ( COMMAND_CHARACTER_SET, &chara );

    loadCurrentChara();
}

int PaletteEditor::getCharaCount()
{
    return _frameDisp->get_character_count() / 3; // Because FrameDisplay has each moon
}

const char *PaletteEditor::getCharaName()
{
    return getShortCharaName ( getCharaIndex() );
}

const char *PaletteEditor::getCharaName ( int chara )
{
    return getShortCharaName ( _frameDisp->get_character_index ( chara * 3 ) ); // Because FrameDisplay has each moon
}

int PaletteEditor::getSpriteNumber()
{
    return _frameDisp->get_sequence();
}

void PaletteEditor::setSpriteNumber ( int spriteNumber )
{
    _frameDisp->command ( COMMAND_SEQUENCE_SET, &spriteNumber );
}

int PaletteEditor::getSpriteFrame()
{
    return _frameDisp->get_subframe();
}

void PaletteEditor::setSpriteFrame ( int spriteFrame )
{
    if ( spriteFrame == _frameDisp->get_subframe() - 1 )
    {
        _frameDisp->command ( COMMAND_FRAME_PREV, 0 );
        return;
    }

    if ( spriteFrame == _frameDisp->get_subframe() + 1 )
    {
        _frameDisp->command ( COMMAND_FRAME_NEXT, 0 );
        return;
    }

    _frameDisp->command ( COMMAND_SUBFRAME_SET, &spriteFrame );
}

void PaletteEditor::nextSpriteSubFrame()
{
    _frameDisp->command ( COMMAND_SUBFRAME_NEXT, 0 );
}

void PaletteEditor::renderSprite()
{
    static const RenderProperties renderProps = { 1, 0, 0, 0, 0, 0 };
    _frameDisp->render ( &renderProps );
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

void PaletteEditor::loadCurrentChara()
{
    if ( _palMans.find ( getChara() ) == _palMans.end() )
    {
        _palMans[getChara()].cache ( static_cast<const MBAACC_FrameDisplay&> ( *_frameDisp ).get_palette_data() );
        _palMans[getChara()].load ( _palettesFolder, getCharaName() );
    }

    _palMans[getChara()].apply ( _frameDisp->get_palette_data() );
}

void PaletteEditor::saveCurrentChara()
{
    if ( _palMans.find ( getChara() ) == _palMans.end() )
        return;

    _mkdir ( _palettesFolder.c_str() );
    _palMans[getChara()].save ( _palettesFolder, getCharaName() );
}

void PaletteEditor::applyColor ( uint32_t color )
{
    uint32_t& currColor = _frameDisp->get_palette_data() [_paletteNumber][_colorNumber];
    const uint32_t newColor = ( currColor & 0xFF000000 ) | ( SWAP_R_AND_B ( color ) & 0xFFFFFF );

    if ( currColor == newColor )
        return;

    currColor = newColor;
    _frameDisp->flush_texture();
}

int PaletteEditor::getCharaIndex()
{
    return _charaNumToIndex[_frameDisp->get_character() / 3]; // Because FrameDisplay has each moon
}
