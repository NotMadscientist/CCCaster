#pragma once

#include "DllAsmHacks.h"


namespace DllHacks
{

extern void *windowHandle;

void initializePreLoad();

void initializePostLoad();

void deinitialize();

}
