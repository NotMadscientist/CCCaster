#pragma once

#include "Utilities.h"

#define HOOK_CALL1_ADDR ( ( char * ) 0x40D032 )
#define LOOP_START_ADDR ( ( char * ) 0x40D330 )
#define HOOK_CALL2_ADDR ( ( char * ) 0x40D411 )

#define INLINE_DWORD_FF { 0xFF, 0x00, 0x00, 0x00 }

const Asm loopStartJump =
{
    LOOP_START_ADDR,
    {
        0xE9, INLINE_DWORD ( HOOK_CALL1_ADDR - LOOP_START_ADDR - 5 ),           // jmp HOOK_CALL1_ADDR
        0x90                                                                    // nop
    }
};

const Asm hookCallback2 =
{
    HOOK_CALL2_ADDR,
    {
        0x6A, 0x01,                                                             // push 01
        0x6A, 0x00,                                                             // push 00
        0x6A, 0x00,                                                             // push 00
        0xE9, INLINE_DWORD ( LOOP_START_ADDR - HOOK_CALL2_ADDR - 5 )            // jmp LOOP_START_ADDR+6
    }
};

// Write a DWORD 0xFF to each
void *const disabledStageAddrs[] =
{
    ( void * ) 0x54CEBC,
    ( void * ) 0x54CEC0,
    ( void * ) 0x54CEC4,
    ( void * ) 0x54CFA8,
    ( void * ) 0x54CFAC,
    ( void * ) 0x54CFB0,
    ( void * ) 0x54CF68,
    ( void * ) 0x54CF6C,
    ( void * ) 0x54CF70,
    ( void * ) 0x54CF74,
    ( void * ) 0x54CF78,
    ( void * ) 0x54CF7C,
    ( void * ) 0x54CF80,
    ( void * ) 0x54CF84,
    ( void * ) 0x54CF88,
    ( void * ) 0x54CF8C,
    ( void * ) 0x54CF90,
    ( void * ) 0x54CF94,
    ( void * ) 0x54CF98,
    ( void * ) 0x54CF9C,
    ( void * ) 0x54CFA0,
    ( void * ) 0x54CFA4
};

const Asm fixRyougiStageMusic1 = { ( void * ) 0x7695F6, { 0x35, 0x00, 0x00, 0x00 } };
const Asm fixRyougiStageMusic2 = { ( void * ) 0x7695EC, { 0xAA, 0xCC, 0x1E, 0x40 } };
