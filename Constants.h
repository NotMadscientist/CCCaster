#pragma once

// Game constants and addresses are prefixed CC

#define CC_VERSION                  "1.4.0"
#define CC_TITLE                    "MELTY BLOOD Actress Again Current Code Ver.1.07 Rev." CC_VERSION
#define CC_STARTUP_TITLE_EN         CC_TITLE " Startup Menu"
#define CC_STARTUP_TITLE_JP         CC_TITLE " ‹N“®ƒƒjƒ…["

#define CC_LOOP_START_ADDR          ( ( char * ) 0x40D330 ) // Start of the main event loop
#define CC_SCREEN_WIDTH_ADDR        ( ( char * ) 0x54D048 ) // The width of the main viewport
#define CC_WORLDTIMER_ADDR          ( ( char * ) 0x55D1D4 ) // uint32_t, always counting up
#define CC_PERF_FREQ_ADDR           ( ( char * ) 0x774A80 ) // Value of QueryPerformanceFrequency for game FPS

// Asm hack are prefixed MM (for modified memory), they should be written to safe locations

#define MM_HOOK_CALL1_ADDR          ( ( char * ) 0x40D032 )
#define MM_HOOK_CALL2_ADDR          ( ( char * ) 0x40D411 )
