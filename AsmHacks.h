#pragma once

#include "Utilities.h"
#include "Constants.h"

#include <vector>


#define INLINE_DWORD(X)                                                         \
    static_cast<unsigned char> ( unsigned ( X ) & 0xFF ),                       \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 8 ) & 0xFF ),              \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 16 ) & 0xFF ),             \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 24 ) & 0xFF )

#define INLINE_DWORD_FF { 0xFF, 0x00, 0x00, 0x00 }

#define INLINE_NOP_TWO_TIMES { 0x90, 0x90 }

#define INLINE_NOP_THREE_TIMES { 0x90, 0x90, 0x90 }

#define HOOK_WINDOWS_FUNC(RETURN_TYPE, FUNC_NAME, ...)                          \
    typedef RETURN_TYPE ( WINAPI *p ## FUNC_NAME ) ( __VA_ARGS__ );             \
    p ## FUNC_NAME o ## FUNC_NAME = 0;                                          \
    RETURN_TYPE WINAPI m ## FUNC_NAME ( __VA_ARGS__ )


// Write to a memory location in the same process, returns 0 on success
int memwrite ( void *dst, const void *src, size_t len );


namespace AsmHacks
{

// Struct for storing assembly code
struct Asm
{
    void *const addr;
    const std::vector<uint8_t> bytes;
    mutable std::vector<uint8_t> backup;

    int write() const;
    int revert() const;
};

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

static const std::vector<Asm> enableDisabledStages =
{
    { ( void * ) 0x54CEBC, INLINE_DWORD_FF },
    { ( void * ) 0x54CEC0, INLINE_DWORD_FF },
    { ( void * ) 0x54CEC4, INLINE_DWORD_FF },
    { ( void * ) 0x54CFA8, INLINE_DWORD_FF },
    { ( void * ) 0x54CFAC, INLINE_DWORD_FF },
    { ( void * ) 0x54CFB0, INLINE_DWORD_FF },
    { ( void * ) 0x54CF68, INLINE_DWORD_FF },
    { ( void * ) 0x54CF6C, INLINE_DWORD_FF },
    { ( void * ) 0x54CF70, INLINE_DWORD_FF },
    { ( void * ) 0x54CF74, INLINE_DWORD_FF },
    { ( void * ) 0x54CF78, INLINE_DWORD_FF },
    { ( void * ) 0x54CF7C, INLINE_DWORD_FF },
    { ( void * ) 0x54CF80, INLINE_DWORD_FF },
    { ( void * ) 0x54CF84, INLINE_DWORD_FF },
    { ( void * ) 0x54CF88, INLINE_DWORD_FF },
    { ( void * ) 0x54CF8C, INLINE_DWORD_FF },
    { ( void * ) 0x54CF90, INLINE_DWORD_FF },
    { ( void * ) 0x54CF94, INLINE_DWORD_FF },
    { ( void * ) 0x54CF98, INLINE_DWORD_FF },
    { ( void * ) 0x54CF9C, INLINE_DWORD_FF },
    { ( void * ) 0x54CFA0, INLINE_DWORD_FF },
    { ( void * ) 0x54CFA4, INLINE_DWORD_FF }
};

static const Asm fixRyougiStageMusic1 = { ( void * ) 0x7695F6, { 0x35, 0x00, 0x00, 0x00 } };
static const Asm fixRyougiStageMusic2 = { ( void * ) 0x7695EC, { 0xAA, 0xCC, 0x1E, 0x40 } };

static const Asm disableFpsLimit = { CC_PERF_FREQ_ADDR, { INLINE_DWORD ( 1 ), INLINE_DWORD ( 0 ) } };

static const std::vector<Asm> hijackControls =
{
    // Disable joystick controls
    { ( void * ) 0x41F098, INLINE_NOP_TWO_TIMES   },
    { ( void * ) 0x41F0A0, INLINE_NOP_THREE_TIMES },
    { ( void * ) 0x4A024E, INLINE_NOP_TWO_TIMES   },
    { ( void * ) 0x4A027F, INLINE_NOP_THREE_TIMES },
    { ( void * ) 0x4A0291, INLINE_NOP_THREE_TIMES },
    { ( void * ) 0x4A02A2, INLINE_NOP_THREE_TIMES },
    { ( void * ) 0x4A02B4, INLINE_NOP_THREE_TIMES },
    { ( void * ) 0x4A02E9, INLINE_NOP_TWO_TIMES   },
    { ( void * ) 0x4A02F2, INLINE_NOP_THREE_TIMES },

    // Zero all keyboard keys
    {
        ( void * ) 0x54D2C0, {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        }
    }
};

} // namespace AsmHacks
