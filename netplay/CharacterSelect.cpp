#include "CharacterSelect.hpp"


uint8_t charaToSelector ( uint8_t chara )
{
    switch ( chara )
    {
        // First row
        case 22: return  2; // Aoko
        case  7: return  3; // Tohno
        case 51: return  4; // Hime
        case 15: return  5; // Nanaya
        case 28: return  6; // Kouma
        // Second row
        case  8: return 10; // Miyako
        case  2: return 11; // Ciel
        case  0: return 12; // Sion
        case 30: return 13; // Ries
        case 11: return 14; // V.Sion
        case  9: return 15; // Wara
        case 31: return 16; // Roa
        // Third row
        case  4: return 19; // Maids
        case  3: return 20; // Akiha
        case  1: return 21; // Arc
        case 19: return 22; // P.Ciel
        case 12: return 23; // Warc
        case 13: return 24; // V.Akiha
        case 14: return 25; // M.Hisui
        // Fourth row
        case 29: return 28; // S.Akiha
        case 17: return 29; // Satsuki
        case 18: return 30; // Len
        case 33: return 31; // Ryougi
        case 23: return 32; // W.Len
        case 10: return 33; // Nero
        case 25: return 34; // NAC
        // Firth row
        case 35: return 38; // KohaMech
        case  5: return 39; // Hisui
        case 20: return 40; // Neko
        case  6: return 41; // Kohaku
        case 34: return 42; // NekoMech
        // Last row
        case RANDOM_CHARACTER: return RANDOM_CHARA_SELECTOR;
    }

    return UNKNOWN_POSITION;
}

uint8_t selectorToChara ( uint8_t chara )
{
    switch ( chara )
    {
        // First row
        case  2: return 22; // Aoko
        case  3: return  7; // Tohno
        case  4: return 51; // Hime
        case  5: return 15; // Nanaya
        case  6: return 28; // Kouma
        // Second row
        case 10: return  8; // Miyako
        case 11: return  2; // Ciel
        case 12: return  0; // Sion
        case 13: return 30; // Ries
        case 14: return 11; // V.Sion
        case 15: return  9; // Wara
        case 16: return 31; // Roa
        // Third row
        case 19: return  4; // Maids
        case 20: return  3; // Akiha
        case 21: return  1; // Arc
        case 22: return 19; // P.Ciel
        case 23: return 12; // Warc
        case 24: return 13; // V.Akiha
        case 25: return 14; // M.Hisui
        // Fourth row
        case 28: return 29; // S.Akiha
        case 29: return 17; // Satsuki
        case 30: return 18; // Len
        case 31: return 33; // Ryougi
        case 32: return 23; // W.Len
        case 33: return 10; // Nero
        case 34: return 25; // NAC
        // Firth row
        case 38: return 35; // KohaMech
        case 39: return  5; // Hisui
        case 40: return 20; // Neko
        case 41: return  6; // Kohaku
        case 42: return 34; // NekoMech
        // Last row
        case RANDOM_CHARA_SELECTOR: return RANDOM_CHARACTER;
    }

    return UNKNOWN_POSITION;
}

const char *getFullCharaName ( uint8_t chara )
{
    switch ( chara )
    {
        // First row
        case 22: return "Aozaki Aoko";
        case  7: return "Tohno Shiki";
        case 51: return "Archetype:Earth";
        case 15: return "Nanaya Shiki";
        case 28: return "Kishima Kouma";
        // Second row
        case  8: return "Miyako Arima";
        case  2: return "Ciel";
        case  0: return "Sion Eltnam Atlasia";
        case 30: return "Riesbyfe Strideberg";
        case 11: return "Sion TATARI";
        case  9: return "Warachia";
        case 31: return "Michael Roa Valdamjong";
        // Third row
        case  4: return "Hisui & Kohaku";
        case  3: return "Tohno Akiha";
        case  1: return "Arcuied Brunestud";
        case 19: return "Powered Ciel";
        case 12: return "Red Arcuied";
        case 13: return "Akiha Vermillion";
        case 14: return "Mech-Hisui";
        // Fourth row
        case 29: return "Seifuku Akiha";
        case 17: return "Yumizuka Satsuki";
        case 18: return "Len";
        case 33: return "Ryougi Shiki";
        case 23: return "White Len";
        case 10: return "Nero Chaos";
        case 25: return "Neko Arc Chaos";
        // Firth row
        case 35: return "Koha & Mech";
        case  5: return "Hisui";
        case 20: return "Neko Arc";
        case  6: return "Kohaku";
        case 34: return "Neko & Mech";
        // Last row
        case RANDOM_CHARACTER: return "Random";
    }

    return "Unknown!";
}

const char *getShortCharaName ( uint8_t chara )
{
    switch ( chara )
    {
        // First row
        case 22: return "Aoko";
        case  7: return "Tohno";
        case 51: return "Hime";
        case 15: return "Nanaya";
        case 28: return "Kouma";
        // Second row
        case  8: return "Miyako";
        case  2: return "Ciel";
        case  0: return "Sion";
        case 30: return "Ries";
        case 11: return "V.Sion";
        case  9: return "Wara";
        case 31: return "Roa";
        // Third row
        case  4: return "Maids";
        case  3: return "Akiha";
        case  1: return "Arc";
        case 19: return "P.Ciel";
        case 12: return "Warc";
        case 13: return "V.Akiha";
        case 14: return "M.Hisui";
        // Fourth row
        case 29: return "S.Akiha";
        case 17: return "Satsuki";
        case 18: return "Len";
        case 33: return "Ryougi";
        case 23: return "W.Len";
        case 10: return "Nero";
        case 25: return "NAC";
        // Firth row
        case 35: return "KohaMech";
        case  5: return "Hisui";
        case 20: return "Neko";
        case  6: return "Kohaku";
        case 34: return "NekoMech";
        // Last row
        case RANDOM_CHARACTER: return "Random";
    }

    return "Unknown!";
}
