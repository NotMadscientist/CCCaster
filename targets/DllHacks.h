#pragma once

#include "AsmHacks.h"

void initializePreLoadHacks();

void initializePostLoadHacks();

void deinitializeHacks();

extern std::string overlayText;
