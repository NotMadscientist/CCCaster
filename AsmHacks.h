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


// DLL callback function
extern "C" void callback();

// Position of the current menu's cursor, this gets updated by ASM hacks
extern uint32_t currentMenuIndex;

// The value of menuConfirmState is set to 1 if a menu confirm happens.
// A menu confirm will only go through if menuConfirmState is greater than 1.
extern uint32_t menuConfirmState;

// Pointers to P1 and P2 character select mode (moon, colour, etc...), this gets updated by ASM hacks
extern uint32_t *charaSelectModePtrs[2];

// Round start counter, this gets incremented whenever players can start moving
extern uint32_t roundStartCounter;

// Auto replay save state, is set to 1 (loading), then 100 (saving), finally 255 (saved).
extern uint32_t *autoReplaySaveStatePtr;


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

typedef std::vector<Asm> AsmList;

// Add a call to the callback function just before the beginning of the game's main message loop.
// Note the message loop can get run multiple times per frame step, so be sure to check the world timer.
static const AsmList hookMainLoop =
{
    // Hook callback function
    { MM_HOOK_CALL1_ADDR, {
        0xE8, INLINE_DWORD ( ( ( char * ) &callback ) - MM_HOOK_CALL1_ADDR - 5 ),   // call callback
        0xE9, INLINE_DWORD ( MM_HOOK_CALL2_ADDR - MM_HOOK_CALL1_ADDR - 10 )         // jmp MM_HOOK_CALL2_ADDR
    } },
    { MM_HOOK_CALL2_ADDR, {
        0x6A, 0x01,                                                                 // push 01
        0x6A, 0x00,                                                                 // push 00
        0x6A, 0x00,                                                                 // push 00
        0xE9, INLINE_DWORD ( CC_LOOP_START_ADDR - MM_HOOK_CALL2_ADDR - 5 )          // jmp CC_LOOP_START_ADDR+6 (after)
    } },

    // Write the jump location last, due to dependencies on the callback hook code
    { CC_LOOP_START_ADDR, {
        0xE9, INLINE_DWORD ( MM_HOOK_CALL1_ADDR - CC_LOOP_START_ADDR - 5 ),         // jmp MM_HOOK_CALL1_ADDR
        0x90                                                                        // nop
                                                                                    // after:
    } },
};

// Enable disabled stages and fix Ryougi stage music looping
static const AsmList enableDisabledStages =
{
    // Enable disabled stages
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
    { ( void * ) 0x54CFA4, INLINE_DWORD_FF },

    // Fix Ryougi stage music looping
    { ( void * ) 0x7695F6, { 0x35, 0x00, 0x00, 0x00 } },
    { ( void * ) 0x7695EC, { 0xAA, 0xCC, 0x1E, 0x40 } },
};

// Disable the FPS limit by setting the game's perceived perf freq to 1.
// TODO find an alternative because this doesn't work on Wine
static const Asm disableFpsLimit = { CC_PERF_FREQ_ADDR, { INLINE_DWORD ( 1 ), INLINE_DWORD ( 0 ) } };

// Disable the code that updates the FPS counter
static const Asm disableFpsCounter = { ( void * ) 0x41FD43, INLINE_NOP_THREE_TIMES };

// Disable normal joystick and keyboard controls
static const AsmList hijackControls =
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
    { ( void * ) 0x54D2C0, {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    } },
};

// Copy dynamic menu variables and hijack the menu confirms
static const AsmList hijackMenu =
{
    // Copy the value of the current menu index (edi) to a location we control.
    // This hack uses the strategy of jumping around the extra spaces left in between code.
    { ( void * ) 0x4294D1, {
        0x8B, 0x7E, 0x40,                                           // mov edi,[esi+40]
        0x89, 0x3D, INLINE_DWORD ( &currentMenuIndex ),             // mov [&currentMenuIndex],edi
        0xE9, 0xF1, 0x04, 0x00, 0x00                                // jmp 0x4299CB+5 (after)
    } },
    { ( void * ) 0x429817, {
        0x85, 0xC9,                                                 // test ecx,ecx
        0xE9, 0xB3, 0xFC, 0xFF, 0xFF                                // jmp 0x4294D1
    } },
    { ( void * ) 0x4299CB, {
        0xE9, 0x47, 0xFE, 0xFF, 0xFF                                // jmp 0x429817
                                                                    // after:
    } },

    // Copy the pointers to the P1 and P2 character select modes to a location we control.
    //
    // Originally this section of code was just:
    //   mov [esi+0x74D8EC],00000001
    //
    // This gets run when a character first gets selected, so we can hijack the address then.
    //
    // In this context:
    //   esi+0x74D8EC is the address that we want
    //   edi is the player index, 0 for P1 and 1 for P2
    //
    // So the modified code saves the address to ecx, assigns [ecx] normally,
    // then copies the value of ecx to a location that we control.
    //
    { ( void * ) 0x42803B, {
        0x51,                                                       // push ecx
        0x8D, 0x8E, 0xEC, 0xD8, 0x74, 0x00,                         // lea ecx,[esi+0x74D8EC]
        0xC7, 0x01, 0x01, 0x00, 0x00, 0x00,                         // mov [ecx],00000001
        0x89, 0x0C, 0xBD, INLINE_DWORD ( charaSelectModePtrs ),     // mov [edi*4+charaSelectModePtrs],ecx
        0x59,                                                       // pop ecx

        // The rest of the assembly code is unmodified and just shifted down
        0xD9, 0x50, 0x2C, 0xC7, 0x00, 0x01, 0x00, 0x00, 0x00, 0xC7, 0x40, 0x24, 0x00, 0x00, 0x00, 0x00,
        0xC7, 0x40, 0x28, 0x00, 0x00, 0x00, 0x00, 0x83, 0xBE, 0x08, 0xD9, 0x74, 0x00, 0x00, 0x75, 0x12,
        0xD9, 0xE8, 0x8B, 0x0D, 0x08, 0xD8, 0x74, 0x00, 0x69, 0xFF, 0xDC, 0x01, 0x00, 0x00, 0xD9, 0x5C,
        0x0F, 0x28, 0xD8, 0x1D, 0x60, 0xE9, 0x76, 0x00, 0x5F, 0x5E, 0xDF, 0xE0, 0xF6, 0xC4, 0x41, 0x7A,
        0x0B, 0x8B, 0x54, 0x24, 0x04, 0xC7, 0x42, 0x48, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x0C, 0x00
    } },

    // Code that allows us to selectively override menu confirms.
    // The value of menuConfirmState is set to 1 if a menu confirm happens.
    // A menu confirm will only go through if menuConfirmState is greater than 1.
    { ( void * ) 0x428F52, {
        0x53, 0x51, 0x52,                                           // push ebx,ecx,edx
        0x8D, 0x5C, 0x24, 0x30,                                     // lea ebx,[esp+30]
        0xB9, INLINE_DWORD ( &menuConfirmState ),                   // mov ecx,&menuConfirmState
        0xEB, 0x04                                                  // jmp 0x428F64
    } },
    { ( void * ) 0x428F64, {
        0x83, 0x39, 0x01,                                           // cmp dword ptr [ecx],01
        0x8B, 0x13,                                                 // mov edx,[ebx]
        0x89, 0x11,                                                 // mov [ecx],edx
        0x7F, 0x5B,                                                 // jg labelB
        0xEB, 0x55                                                  // jmp labelA
    } },
    { ( void * ) 0x428F7A, {
        0x5A, 0x59, 0x5B,                                           // pop edx,ecx,ebx
        0x90,                                                       // nop
        0xEB, 0x0D                                                  // jmp RETURN
    } },
    { ( void * ) 0x428F82, {
        0x81, 0x3C, 0x24, 0xF5, 0x99, 0x42, 0x00,                   // cmp [esp],0x4299F5 (return addr of menu call)
        0x75, 0x02,                                                 // jne RETURN
        0xEB, 0xC5,                                                 // jmp 0x428F52
                                                                    // RETURN:
        0xC2, 0x04, 0x00                                            // ret 0004
    } },
    { ( void * ) 0x428FC4, {
                                                                    // labelA:
        0x89, 0x03,                                                 // mov [ebx],eax
        0xEB, 0xB2,                                                 // jmp 0x428F7A
                                                                    // labelB:
        0x89, 0x01,                                                 // mov [ecx],eax
        0xEB, 0xAE                                                  // jmp 0x428F7A
    } },
};

static const AsmList detectRoundStart =
{
    // Increment a counter at the beginning of the round when players can move
    { ( void * ) 0x440CC5, {
        0xEB, 0x4F                                                  // jmp 0x440D16
                                                                    // after:
    } },
    { ( void * ) 0x440D16, {
        0xB9, INLINE_DWORD ( &roundStartCounter ),                  // mov ecx,[&roundStartCounter]
        0xE9, 0xE2, 0x02, 0x00, 0x00                                // jmp 0x441002
    } },
    { ( void * ) 0x441002, {
        0x8B, 0x31,                                                 // mov esi,[ecx]
        0x46,                                                       // inc esi
        0x89, 0x31,                                                 // mov [ecx],esi
        0x5E,                                                       // pop esi
        0x59,                                                       // pop ecx
        0xE9, 0x07, 0xFD, 0xFF, 0xFF                                // jmp 0x440CC5+2 (after)
    } },
};

// This copies an auto replay save flag to a non-dynamic memory location.
// Used to detect when the auto replay save is done and the menu is up.
static const Asm detectAutoReplaySave =
    { ( void * ) 0x482D9B, {
        0x8D, 0x88, 0xD0, 0x00, 0x00, 0x00,                         // lea ecx,[eax+000000D0]
        0x89, 0x0D, INLINE_DWORD ( &autoReplaySaveStatePtr ),       // mov [&autoReplaySaveStatePtr],ecx
        0x59, 0x5E, 0x83, 0xC4, 0x10, 0xC2, 0x04, 0x00              // rest of the code is just shifted down
    } };

} // namespace AsmHacks
