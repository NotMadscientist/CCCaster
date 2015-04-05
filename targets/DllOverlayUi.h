#pragma once

#include <string>
#include <array>


#define DEFAULT_MESSAGE_TIMEOUT ( 3000 )


namespace DllOverlayUi
{

void init();


void enable();

void disable();

void toggle();

bool isEnabled();

bool isToggling();

void updateText();

void updateText ( const std::array<std::string, 3>& text );

void updateSelector ( uint8_t index, int position = 0, const std::string& line = "" );


void showMessage ( const std::string& text, int timeout = DEFAULT_MESSAGE_TIMEOUT );

void updateMessage();

bool isShowingMessage();


#ifndef RELEASE

extern std::string debugText;

extern int debugTextAlign;

#endif // NOT RELEASE

}
