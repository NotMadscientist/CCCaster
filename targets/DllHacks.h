#pragma once

#include "AsmHacks.h"

#include <string>


namespace DllHacks
{

extern void *mainWindowHandle;


void initializePreLoad();

void initializePostLoad();

void deinitialize();

} // namespace DllHacks
