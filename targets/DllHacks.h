#pragma once

#include "AsmHacks.h"

#include <string>
#include <array>


namespace DllHacks
{

extern void *mainWindowHandle;


void enableOverlay();

void disableOverlay();

void toggleOverlay();

void updateOverlay ( const std::array<std::string, 3>& newText );

bool isOverlayEnabled();


void initializePreLoad();

void initializePostLoad();

void deinitialize();

} // namespace DllHacks
