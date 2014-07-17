#pragma once

#include "Utilities.h"

// Game constants and addresses are prefixed CC
#define CC_LOOP_START_ADDR  ( ( char * ) 0x40D330 ) // Start of the main event loop
#define CC_PERF_FREQ_ADDR   ( ( char * ) 0x774A80 ) // Value of QueryPerformanceFrequency for game FPS

// Asm hack are prefix MM (for modified memory), they should be written to safe locations
#define MM_HOOK_CALL1_ADDR  ( ( char * ) 0x40D032 )
#define MM_HOOK_CALL2_ADDR  ( ( char * ) 0x40D411 )

#define INLINE_DWORD_FF { 0xFF, 0x00, 0x00, 0x00 }

namespace AsmHacks
{

static const Asm loopStartJump =
{
    CC_LOOP_START_ADDR,
    {
        0xE9, INLINE_DWORD ( MM_HOOK_CALL1_ADDR - CC_LOOP_START_ADDR - 5 ),     // jmp MM_HOOK_CALL1_ADDR
        0x90                                                                    // nop
    }
};

static const Asm hookCallback2 =
{
    MM_HOOK_CALL2_ADDR,
    {
        0x6A, 0x01,                                                             // push 01
        0x6A, 0x00,                                                             // push 00
        0x6A, 0x00,                                                             // push 00
        0xE9, INLINE_DWORD ( CC_LOOP_START_ADDR - MM_HOOK_CALL2_ADDR - 5 )      // jmp CC_LOOP_START_ADDR+6
    }
};

// Write a DWORD 0xFF to each
static void *const disabledStageAddrs[] =
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

static const Asm fixRyougiStageMusic1 = { ( void * ) 0x7695F6, { 0x35, 0x00, 0x00, 0x00 } };
const Asm fixRyougiStageMusic2 = { ( void * ) 0x7695EC, { 0xAA, 0xCC, 0x1E, 0x40 } };

static const Asm disableFpsLimit = { CC_PERF_FREQ_ADDR, { INLINE_DWORD ( 1 ), INLINE_DWORD ( 0 ) } };

} // namespace AsmHacks
