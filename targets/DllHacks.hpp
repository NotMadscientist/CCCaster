#pragma once

#include "DllAsmHacks.hpp"


namespace DllHacks
{

extern void *windowHandle;

void initializePreLoad();

void initializePostLoad();

void deinitialize();

}
