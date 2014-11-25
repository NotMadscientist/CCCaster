#pragma once

#include "ErrorStrings.h"


#define ERROR_PIPE_OPEN             "Failed to start game!\nIs " MBAA_EXE " already running?"

#define ERROR_PIPE_START            "Failed to start game!\nIs " BINARY " in same folder as " MBAA_EXE "?"

#define ERROR_PIPE_RW               "Failed to communicate with " MBAA_EXE

#define ERROR_INVALID_GAME_MODE     "Unhandled game mode!"

#define ERROR_INVALID_HOST_CONFIG   "Host sent invalid configuration!"

#define ERROR_INVALID_NETPLAY_DELAY "Delay must be between 0 and 255!"

#define ERROR_INVALID_OFFLINE_DELAY "Delay must be less than 255!"
