#pragma once

#include "AsmHacks.h"


namespace DllHacks
{

extern void *windowHandle;

void initializePreLoad();

void initializePostLoad();

void deinitialize();

}
