#pragma once

#include "ErrorStrings.h"


#define ERROR_PIPE_START            "Failed to start game!\nIs " BINARY " in same folder as " MBAA_EXE "?"

#define ERROR_PIPE_RW               "Failed to communicate with " MBAA_EXE

#define ERROR_INVALID_GAME_MODE     "Unhandled game mode!"

#define ERROR_INVALID_HOST_CONFIG   "Host sent invalid configuration!"

#define ERROR_INVALID_DELAY         "Delay must be less than 255!"
