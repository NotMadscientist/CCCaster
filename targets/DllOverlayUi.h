#pragma once

#include <string>
#include <array>


namespace DllOverlayUi
{

void enable();

void disable();

void toggle();

void updateText ( const std::array<std::string, 3>& newText );

void updateSelector ( uint8_t index, int position = 0, const std::string& line = "" );

bool isEnabled();

}
