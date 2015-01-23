#pragma once

#include <cstdint>
#include <iostream>

#include "Controller.h"


// Number of frames of inputs to send per message
#define NUM_INPUTS                  ( 30 )
#define MAX_ROLLBACK                ( 9 )


// Game constants and addresses are prefixed CC
#define CC_VERSION                  "1.4.0"
#define CC_TITLE                    "MELTY BLOOD Actress Again Current Code Ver.1.07 Rev." CC_VERSION
#define CC_STARTUP_TITLE            CC_TITLE " "
#define CC_STARTUP_BUTTON           "OK"
#define CC_NETWORK_CONFIG_FILE      "System\\NetConnect.dat"
#define CC_NETWORK_USERNAME_KEY     "UserName"

// Location of the keyboard config in the binary
#define CC_KEYBOARD_CONFIG_OFFSET   ( 0x14D2C0 )

#define CC_WINDOW_PROC_ADDR         ( ( char * )     0x40D4C0 ) // Location of WindowProc
#define CC_LOOP_START_ADDR          ( ( char * )     0x40D330 ) // Start of the main event loop
#define CC_SCREEN_WIDTH_ADDR        ( ( uint32_t * ) 0x54D048 ) // The actual width of the main viewport
#define CC_DAMAGE_LEVEL_ADDR        ( ( uint32_t * ) 0x553FCC ) // Damage level: default 2
#define CC_WIN_COUNT_VS_ADDR        ( ( uint32_t * ) 0x553FDC ) // Win count: default 2
#define CC_TIMER_SPEED_ADDR         ( ( uint32_t * ) 0x553FD0 ) // Timer speed: default 2
#define CC_AUTO_REPLAY_SAVE_ADDR    ( ( uint32_t * ) 0x553FE8 ) // Auto replay saving: 0 to disable, 1 to enable
#define CC_WORLD_TIMER_ADDR         ( ( uint32_t * ) 0x55D1D4 ) // Frame step timer, always counting up
#define CC_PAUSE_FLAG_ADDR          ( ( uint8_t * )  0x55D203 ) // 1 when paused
#define CC_SKIP_FRAMES_ADDR         ( ( uint32_t * ) 0x55D25C ) // Set to N to disable FPS limit for N frames
#define CC_ROUND_TIMER_ADDR         ( ( uint32_t * ) 0x562A3C ) // Counts down from 4752, may stop
#define CC_REAL_TIMER_ADDR          ( ( uint32_t * ) 0x562A40 ) // Counts up from 0 after round start
#define CC_TRAINING_PAUSE_ADDR      ( ( uint32_t * ) 0x562A64 ) // 1 when paused
#define CC_VERSUS_PAUSE_ADDR        ( ( uint32_t * ) 0x564B30 ) // 0xFFFFFFFF when paused
#define CC_P1_GAME_POINT_FLAG_ADDR  ( ( uint32_t * ) 0x559548 ) // P1 game point flag
#define CC_P2_GAME_POINT_FLAG_ADDR  ( ( uint32_t * ) 0x55954C ) // P2 game point flag
#define CC_P1_WINS_ADDR             ( ( uint32_t * ) 0x559550 ) // P1 number of wins
#define CC_P2_WINS_ADDR             ( ( uint32_t * ) 0x559580 ) // P2 number of wins
#define CC_ROUND_COUNT_ADDR         ( ( uint32_t * ) 0x5550E0 ) // Round count
#define CC_INTRO_STATE_ADDR         ( ( uint8_t * )  0x55D20B ) // 2 (character intros), 1 (pre-game), 0 (in-game)
#define CC_HIT_SPARKS_ADDR          ( ( uint32_t * ) 0x67BD78 ) // Number of hit sparks?

#define CC_STAGE_SELECTOR_ADDR      ( ( uint32_t * ) 0x74FD98 ) // Currently selected stage, can be assigned to directly
#define CC_FPS_COUNTER_ADDR         ( ( uint32_t * ) 0x774A70 ) // Value of the displayed FPS counter
#define CC_PERF_FREQ_ADDR           ( ( uint64_t * ) 0x774A80 ) // Value of QueryPerformanceFrequency for game FPS
#define CC_SKIPPABLE_FLAG_ADDR      ( ( uint32_t * ) 0x74D99C ) // Flag that indicates a skippable state when in-game
#define CC_ALIVE_FLAG_ADDR          ( ( uint8_t * )  0x76E650 ) // Flag that indicates the game is alive

// Some menu state counter, incremented for each open menu, decremented when menu closes
#define CC_MENU_STATE_COUNTER_ADDR  ( ( uint32_t * ) 0x767440 )

#define CC_DUMMY_STATUS_ADDR        ( ( int32_t * )  0x74D7F8 ) // Training mode dummy status
#define CC_DUMMY_STATUS_STAND       ( 0 )
#define CC_DUMMY_STATUS_JUMP        ( 1 )
#define CC_DUMMY_STATUS_CROUCH      ( 2 )
#define CC_DUMMY_STATUS_CPU         ( 3 )
#define CC_DUMMY_STATUS_MANUAL      ( 4 )
#define CC_DUMMY_STATUS_DUMMY       ( 5 )
#define CC_DUMMY_STATUS_RECORD      ( -1 )

#define CC_PTR_TO_WRITE_INPUT_ADDR  ( ( char * )     0x76E6AC ) // Pointer to the location to write game input
#define CC_P1_OFFSET_DIRECTION      ( 0x18 )                    // Offset to write P1 direction input
#define CC_P1_OFFSET_BUTTONS        ( 0x24 )                    // Offset to write P1 buttons input
#define CC_P2_OFFSET_DIRECTION      ( 0x2C )                    // Offset to write P2 direction input
#define CC_P2_OFFSET_BUTTONS        ( 0x38 )                    // Offset to write P2 buttons input

// Directions are just written in numpad format, EXCEPT neutral is 0
#define CC_BUTTON_A                 ( 0x0010 )
#define CC_BUTTON_B                 ( 0x0020 )
#define CC_BUTTON_C                 ( 0x0008 )
#define CC_BUTTON_D                 ( 0x0004 )
#define CC_BUTTON_E                 ( 0x0080 )
#define CC_BUTTON_AB                ( 0x0040 )
#define CC_BUTTON_START             ( 0x0001 )
#define CC_BUTTON_FN1               ( 0x0100 ) // Control dummy
#define CC_BUTTON_FN2               ( 0x0200 ) // Training reset
#define CC_BUTTON_CONFIRM           ( 0x0400 )
#define CC_BUTTON_CANCEL            ( 0x0800 )

#define CC_GAME_MODE_ADDR           ( ( uint32_t * ) 0x54EEE8 ) // Current game mode, constants below

// List of game modes relevant to netplay
#define CC_GAME_MODE_STARTUP        ( 65535 )
#define CC_GAME_MODE_OPENING        ( 3 )
#define CC_GAME_MODE_TITLE          ( 2 )
#define CC_GAME_MODE_LOADING_DEMO   ( 13 )
#define CC_GAME_MODE_HIGH_SCORES    ( 11 )
#define CC_GAME_MODE_MAIN           ( 25 )
#define CC_GAME_MODE_CHARA_SELECT   ( 20 )
#define CC_GAME_MODE_LOADING        ( 8 )
#define CC_GAME_MODE_IN_GAME        ( 1 )
#define CC_GAME_MODE_RETRY          ( 5 )

// Character select data, can be assigned to directly at the character select screen
#define CC_P1_SELECTOR_MODE_ADDR    ( ( uint32_t * ) 0x74D8EC )
#define CC_P1_CHARA_SELECTOR_ADDR   ( ( uint32_t * ) 0x74D8F8 )
#define CC_P1_CHARACTER_ADDR        ( ( uint32_t * ) 0x74D8FC )
#define CC_P1_MOON_SELECTOR_ADDR    ( ( uint32_t * ) 0x74D900 )
#define CC_P1_COLOR_SELECTOR_ADDR   ( ( uint32_t * ) 0x74D904 )
#define CC_P1_RANDOM_COLOR_ADDR     ( ( uint8_t * ) ( ( * ( uint32_t * ) 0x74D808 ) + 0 * 0x1DC + 0x2C + 0x0C ) )
#define CC_P2_SELECTOR_MODE_ADDR    ( ( uint32_t * ) 0x74D910 )
#define CC_P2_CHARA_SELECTOR_ADDR   ( ( uint32_t * ) 0x74D91C )
#define CC_P2_CHARACTER_ADDR        ( ( uint32_t * ) 0x74D920 )
#define CC_P2_MOON_SELECTOR_ADDR    ( ( uint32_t * ) 0x74D924 )
#define CC_P2_COLOR_SELECTOR_ADDR   ( ( uint32_t * ) 0x74D928 )
#define CC_P2_RANDOM_COLOR_ADDR     ( ( uint8_t * ) ( ( * ( uint32_t * ) 0x74D808 ) + 1 * 0x1DC + 0x2C + 0x0C ) )

// Complete RngState
#define CC_RNG_STATE0_ADDR          ( ( uint32_t * ) 0x563778 )
#define CC_RNG_STATE1_ADDR          ( ( uint32_t * ) 0x56377C )
#define CC_RNG_STATE2_ADDR          ( ( uint32_t * ) 0x564068 )
#define CC_RNG_STATE3_ADDR          ( ( char * )     0x564070 )
#define CC_RNG_STATE3_SIZE          ( 220 )

// Character select selection mode
#define CC_SELECT_CHARA             ( 0 )
#define CC_SELECT_MOON              ( 1 )
#define CC_SELECT_COLOR             ( 2 )

// Total size of a single player structure.
// Note: there are FOUR player structs in memory, due to the puppet characters.
#define CC_PLR_STRUCT_SIZE          ( 0xAFC )

// Player addresses
// P1 start from 0x555134
// P2 start from 0x555C30
#define CC_P1_SEQUENCE_ADDR         ( ( uint32_t * ) 0x555140 )
#define CC_P1_SEQ_STATE_ADDR        ( ( uint32_t * ) 0x555144 )
#define CC_P1_HEALTH_ADDR           ( ( uint32_t * ) 0x5551EC )
#define CC_P1_RED_HEALTH_ADDR       ( ( uint32_t * ) 0x5551F0 )
#define CC_P1_GUARD_BAR_ADDR        ( ( float * )    0x5551F4 )
#define CC_P1_GUARD_QUALITY_ADDR    ( ( float * )    0x555208 )
#define CC_P1_METER_ADDR            ( ( uint32_t * ) 0x555210 )
#define CC_P1_HEAT_ADDR             ( ( uint32_t * ) 0x555214 )
#define CC_P1_NO_INPUT_FLAG_ADDR    ( ( uint8_t * )  0x5552A7 ) // Indicates when input is disabled, ie KO or time over

#define CC_P1_X_POSITION_ADDR       ( ( int32_t * )  0x555238 )
#define CC_P1_Y_POSITION_ADDR       ( ( int32_t * )  0x55523C )
#define CC_P1_X_PREV_POS_ADDR       ( ( int32_t * )  0x555244 )
#define CC_P1_Y_PREV_POS_ADDR       ( ( int32_t * )  0x555248 )
#define CC_P1_X_VELOCITY_ADDR       ( ( int32_t * )  0x55524C )
#define CC_P1_Y_VELOCITY_ADDR       ( ( int32_t * )  0x555250 )
#define CC_P1_X_ACCELERATION_ADDR   ( ( int16_t * )  0x555254 )
#define CC_P1_Y_ACCELERATION_ADDR   ( ( int16_t * )  0x555256 )
#define CC_P1_SPRITE_ANGLE_ADDR     ( ( uint32_t * ) 0x555430 )
#define CC_P1_FACING_FLAG_ADDR      ( ( uint8_t * )  0x555444 ) // 0 facing left, 1 facing right

#define CC_P2_SEQUENCE_ADDR         ( ( uint32_t * ) ( ( ( char * ) CC_P1_SEQUENCE_ADDR      ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_SEQ_STATE_ADDR        ( ( uint32_t * ) ( ( ( char * ) CC_P1_SEQ_STATE_ADDR     ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_HEALTH_ADDR           ( ( uint32_t * ) ( ( ( char * ) CC_P1_HEALTH_ADDR        ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_RED_HEALTH_ADDR       ( ( uint32_t * ) ( ( ( char * ) CC_P1_RED_HEALTH_ADDR    ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_GUARD_BAR_ADDR        ( ( float * )    ( ( ( char * ) CC_P1_GUARD_BAR_ADDR     ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_GUARD_QUALITY_ADDR    ( ( float * )    ( ( ( char * ) CC_P1_GUARD_QUALITY_ADDR ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_METER_ADDR            ( ( uint32_t * ) ( ( ( char * ) CC_P1_METER_ADDR         ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_HEAT_ADDR             ( ( uint32_t * ) ( ( ( char * ) CC_P1_HEAT_ADDR          ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_NO_INPUT_FLAG_ADDR    ( ( uint8_t * )  ( ( ( char * ) CC_P1_NO_INPUT_FLAG_ADDR ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_X_POSITION_ADDR       ( ( int32_t * )  ( ( ( char * ) CC_P1_X_POSITION_ADDR    ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_Y_POSITION_ADDR       ( ( int32_t * )  ( ( ( char * ) CC_P1_Y_POSITION_ADDR    ) + CC_PLR_STRUCT_SIZE ) )
#define CC_P2_FACING_FLAG_ADDR      ( ( uint8_t * )  ( ( ( char * ) CC_P1_FACING_FLAG_ADDR   ) + CC_PLR_STRUCT_SIZE ) )

// Camera addresses
#define CC_CAMERA_X_ADDR            ( ( int * )      0x564B14 )
#define CC_CAMERA_Y_ADDR            ( ( int * )      0x564B18 )

// Array of sound effect flags, each byte corresponds to a specific SFX, set to 1 to start
#define CC_SFX_ARRAY_ADDR           ( ( uint8_t * )  0x76E008 )
#define CC_SFX_ARRAY_LEN            ( 1500 )


// Asm hacks are prefixed MM (for modified memory), they should be written to safe locations
#define MM_HOOK_CALL1_ADDR          ( ( char * )     0x40D032 )
#define MM_HOOK_CALL2_ADDR          ( ( char * )     0x40D411 )


union IndexedFrame
{
    struct { uint32_t frame, index; } parts;
    uint64_t value;
};

const IndexedFrame MaxIndexedFrame = {{ UINT_MAX, UINT_MAX }};

inline std::ostream& operator<< ( std::ostream& os, const IndexedFrame& indexedFrame )
{
    return ( os << indexedFrame.parts.index << ':' << indexedFrame.parts.frame );
}


inline const char *gameModeStr ( uint32_t gameMode )
{
    switch ( gameMode )
    {
        case CC_GAME_MODE_STARTUP:
            return "Startup";

        case CC_GAME_MODE_OPENING:
            return "Opening";

        case CC_GAME_MODE_TITLE:
            return "Title";

        case CC_GAME_MODE_LOADING_DEMO:
            return "Loading-demo";

        case CC_GAME_MODE_HIGH_SCORES:
            return "High-scores";

        case CC_GAME_MODE_MAIN:
            return "Main";

        case CC_GAME_MODE_CHARA_SELECT:
            return "Character-select";

        case CC_GAME_MODE_LOADING:
            return "Loading";

        case CC_GAME_MODE_IN_GAME:
            return "In-game";

        case CC_GAME_MODE_RETRY:
            return "Retry";

        default:
            break;
    }

    return "Unknown game mode!";
}


const std::vector<std::pair<std::string, uint32_t>> gameInputBits =
{
    { "Up",          BIT_UP },
    { "Down",        BIT_DOWN },
    { "Left",        BIT_LEFT },
    { "Right",       BIT_RIGHT },
    { "A (confirm)", ( CC_BUTTON_A | CC_BUTTON_CONFIRM ) << 8 },
    { "B (cancel)",  ( CC_BUTTON_B | CC_BUTTON_CANCEL ) << 8 },
    { "C",           CC_BUTTON_C << 8 },
    { "D",           CC_BUTTON_D << 8 },
    { "E",           CC_BUTTON_E << 8 },
    { "Start",       CC_BUTTON_START << 8 },
    { "FN1",         CC_BUTTON_FN1 << 8 },
    { "FN2",         CC_BUTTON_FN2 << 8 },
    { "A+B",         CC_BUTTON_AB << 8 },
};
