static const char *fullCharaName ( uint32_t chara )
{
    switch ( chara )
    {
        // First row
        case 2: return "Aozaki Aoko";
        case 3: return "Tohno Shiki";
        case 4: return "Archetype:Earth";
        case 5: return "Nanaya Shiki";
        case 6: return "Kishima Kouma";
        // Second row
        case 10: return "Arima Miyako";
        case 11: return "Ciel";
        case 12: return "Sion Eltnam Atlasia";
        case 13: return "Riesbyfe Strideberg";
        case 14: return "Sion TATARI";
        case 15: return "Warachia";
        case 16: return "Michael Roa Valdamjong";
        // Third row
        case 19: return "Hisui & Kohaku";
        case 20: return "Tohno Akiha";
        case 21: return "Arcuied Brunestud";
        case 22: return "Powered Ciel";
        case 23: return "Red Arcuied";
        case 24: return "Akiha Vermillion";
        case 25: return "Mech-Hisui";
        // Fourth row
        case 28: return "Seifuku Akiha";
        case 29: return "Yumizuka Satsuki";
        case 30: return "Len";
        case 31: return "Ryougi Shiki";
        case 32: return "White Len";
        case 33: return "Nero Chaos";
        case 34: return "Neko Arc Chaos";
        // Firth row
        case 38: return "Koha & Mech";
        case 39: return "Hisui";
        case 40: return "Neko Arc";
        case 41: return "Kohaku";
        case 42: return "Neko & Mech";
        // Last row
        case 49: return "Random";
    }

    return "Unknown!";
}

static const char *shortCharaName ( uint32_t chara )
{
    switch ( chara )
    {
        // First row
        case 2: return "Aoko";
        case 3: return "Tohno";
        case 4: return "Hime";
        case 5: return "Nanaya";
        case 6: return "Kouma";
        // Second row
        case 10: return "Miyako";
        case 11: return "Ciel";
        case 12: return "Sion";
        case 13: return "Ries";
        case 14: return "V.Sion";
        case 15: return "Wara";
        case 16: return "Roa";
        // Third row
        case 19: return "Maids";
        case 20: return "Akiha";
        case 21: return "Arc";
        case 22: return "P.Ciel";
        case 23: return "Warc";
        case 24: return "V.Akiha";
        case 25: return "M.Hisui";
        // Fourth row
        case 28: return "S.Akiha";
        case 29: return "Satsuki";
        case 30: return "Len";
        case 31: return "Ryougi";
        case 32: return "W.Len";
        case 33: return "Nero";
        case 34: return "MAC";
        // Firth row
        case 38: return "KohaMech";
        case 39: return "Hisui";
        case 40: return "Neko";
        case 41: return "Kohaku";
        case 42: return "NekoMech";
        // Last row
        case 49: return "Random";
    }

    return "Unknown!";
}
