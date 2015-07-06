#pragma once

#include <cstdint>


#define RANDOM_CHARACTER        ( 99 )

#define RANDOM_CHARA_SELECTOR   ( 49 )

#define UNKNOWN_POSITION        ( 0xFF )


uint8_t charaToSelector ( uint8_t chara );

uint8_t selectorToChara ( uint8_t selector );

const char *getFullCharaName ( uint8_t chara );

const char *getShortCharaName ( uint8_t chara );
