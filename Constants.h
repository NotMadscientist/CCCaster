#pragma once

// Game constants and addresses are prefixed CC

#define CC_VERSION                  "1.4.0"
#define CC_TITLE                    "MELTY BLOOD Actress Again Current Code Ver.1.07 Rev." CC_VERSION
#define CC_STARTUP_TITLE_EN         CC_TITLE " Startup Menu"
#define CC_STARTUP_TITLE_JP         CC_TITLE " ‹N“®ƒƒjƒ…["

#define CC_LOOP_START_ADDR          ( ( char * )     0x40D330 ) // Start of the main event loop
#define CC_SCREEN_WIDTH_ADDR        ( ( uint32_t * ) 0x54D048 ) // The width of the main viewport
#define CC_WORLD_TIMER_ADDR         ( ( uint32_t * ) 0x55D1D4 ) // Frame step timer, always counting up
#define CC_FPS_COUNTER_ADDR         ( ( uint32_t * ) 0x774A70 ) // Value of the displayed FPS counter
#define CC_PERF_FREQ_ADDR           ( ( uint64_t * ) 0x774A80 ) // Value of QueryPerformanceFrequency for game FPS

#define CC_MAX_MENU_INDEX           ( 20 )

#define CC_PTR_TO_WRITE_INPUTS_ADDR ( ( char * ) 0x76E6AC ) // Pointer to the location to write inputs
#define CC_P1_OFFSET_DIRECTION      ( 0x18 )                // Offset to write P1 direction inputs
#define CC_P1_OFFSET_BUTTONS        ( 0x24 )                // Offset to write P1 button inputs
#define CC_P2_OFFSET_DIRECTION      ( 0x2C )                // Offset to write P2 direction inputs
#define CC_P2_OFFSET_BUTTONS        ( 0x38 )                // Offset to write P2 button inputs

// Directions are just written in numpad format, EXCEPT neutral is 0

#define CC_BUTTON_A                 ( 0x0010 )
#define CC_BUTTON_B                 ( 0x0020 )
#define CC_BUTTON_C                 ( 0x0008 )
#define CC_BUTTON_D                 ( 0x0004 )
#define CC_BUTTON_E                 ( 0x0080 )
#define CC_BUTTON_AB                ( 0x0040 )
#define CC_BUTTON_START             ( 0x0001 )
#define CC_BUTTON_FN1               ( 0x0100 )
#define CC_BUTTON_FN2               ( 0x0200 )
#define CC_BUTTON_SELECT            ( 0x0400 )
#define CC_BUTTON_CANCEL            ( 0x0800 )

// Asm hack are prefixed MM (for modified memory), they should be written to safe locations

#define MM_HOOK_CALL1_ADDR          ( ( char * ) 0x40D032 )
#define MM_HOOK_CALL2_ADDR          ( ( char * ) 0x40D411 )
