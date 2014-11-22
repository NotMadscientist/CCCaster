#pragma once

#include "AsmHacks.h"

#include <string>


namespace DllHacks
{

extern std::string overlayText[3];

extern void *mainWindowHandle;


void enableOverlay();

void disableOverlay();

void toggleOverlay();

bool isOverlayEnabled();


void initializePreLoad();

void initializePostLoad();

void deinitialize();

} // namespace DllHacks
