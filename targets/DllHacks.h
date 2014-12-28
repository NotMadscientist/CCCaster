#pragma once

#include "AsmHacks.h"


#define WRITE_ASM_HACK(ASM_HACK)                                                                                    \
    do {                                                                                                            \
        int error = ASM_HACK.write();                                                                               \
        if ( error != 0 ) {                                                                                         \
            LOG ( "[%d] %s; %s failed; addr=%08x",                                                                  \
                  error, WinException::getAsString ( error ), #ASM_HACK, ASM_HACK.addr );                           \
            exit ( -1 );                                                                                            \
        }                                                                                                           \
    } while ( 0 )


namespace DllHacks
{

extern void *windowHandle;

void initializePreLoad();

void initializePostLoad();

void deinitialize();

}
