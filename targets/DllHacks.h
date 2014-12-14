#pragma once

#include "AsmHacks.h"

#include <string>
#include <array>


#define WRITE_ASM_HACK(ASM_HACK)                                                                                    \
    do {                                                                                                            \
        int error = ASM_HACK.write();                                                                               \
        if ( error != 0 ) {                                                                                         \
            LOG ( "%s; %s failed; addr=%08x", WinException::getAsString ( error ), #ASM_HACK, ASM_HACK.addr );      \
            exit ( -1 );                                                                                            \
        }                                                                                                           \
    } while ( 0 )


namespace DllHacks
{

extern void *windowHandle;

extern volatile bool keyboardManagerHooked;


void enableOverlay();

void disableOverlay();

void toggleOverlay();

void updateOverlay ( const std::array<std::string, 3>& newText );

void updateSelector ( uint8_t index, int position = 0, const std::string& line = "" );

bool isOverlayEnabled();


void initializePreLoad();

void initializePostLoad();

void deinitialize();

} // namespace DllHacks
