#include "MemDump.h"
#include "Constants.h"

#include <utility>
#include <algorithm>

using namespace std;


#define LOG_FILE "generator.log"


#define CC_UNKNOWN_TIMER_ADDR       ((char *)0x55D1CC) // 4 bytes
#define CC_OUTROSTATE_ADDR          ((char *)0x563948) // 4 bytes, start from 2, goes to 3 (for timeouts?)

#define CC_PLR_ARRAY_ADDR           ((char *)0x555134) // player info
#define CC_PLR_STRUCT_LEN           (2812)
#define CC_PLR_STRUCT_HEADER        (12)               // stuff that doesn't change

#define CC_HUD_ARRAY_ADDR           ((char *)0x557DD8) // combo info
#define CC_HUD_STRUCT_LEN           (524)

#define CC_CAMERA_XY1_ADDR          ((char *)0x555124) // 8 bytes, unstable
#define CC_CAMERA_XY2_ADDR          ((char *)0x5585E8) // 8 bytes

#define CC_P1_SUPER_TIMER1_ADDR     ((char *)0x558684) // 8 bytes, L then R timer
#define CC_P1_SUPER_TIMER2_ADDR     ((char *)0x558784) // 8 bytes, L then R timer
#define CC_P1_SUPER_TIMER3_ADDR     ((char *)0x558884) // 8 bytes, L then R timer
#define CC_P1_SUPER_TIMER4_ADDR     ((char *)0x558908) // 4 bytes
#define CC_P1_SUPER_TIMER5_ADDR     ((char *)0x558910) // 4 bytes

#define CC_P2_SUPER_TIMER1_ADDR     ((char *)0x558990) // 8 bytes, L then R timer
#define CC_P2_SUPER_TIMER2_ADDR     ((char *)0x558A90) // 8 bytes, L then R timer
#define CC_P2_SUPER_TIMER3_ADDR     ((char *)0x558B90) // 8 bytes, L then R timer
#define CC_P2_SUPER_TIMER4_ADDR     ((char *)0x558C14) // 4 bytes
#define CC_P2_SUPER_TIMER5_ADDR     ((char *)0x558C1C) // 4 bytes

// 0x559550 - P1 wins
// 0x559580 - P2 wins
// 0x5595B4 - super flash flag
#define CC_UNKNOWN_ADDR_START       ((char *)0x559550)
#define CC_UNKNOWN_ADDR_END         ((char *)0x55B3C8)

#define CC_UNKNOWN2_ADDR_START      ((char *)0x55DEA0)
#define CC_UNKNOWN2_ADDR_END        ((char *)0x55DF30)

// 0x562A3C - round timer
// 0x562A40 - real timer
// 0x562A48 - super flash timer
// 0x562A58 - ??? timer
// 0x562A5C - game mode?
// 0x562A6C - round over state
#define CC_TIMERS_ADDR_START        ((char *)0x562A3C)
#define CC_TIMERS_ADDR_END          ((char *)0x562A70)

// P1 and P2 status messages
// - indicates rebeat, crits, max, circuit break, etc...
// #define CC_MSG1_ARRAY_ADDR          ((char *)0x563580)
// #define CC_MSG2_ARRAY_ADDR          ((char *)0x5635F4)
// #define CC_MSG_ARRAY_LEN            (96)
#define CC_MSGS_ADDR_START          ((char *)0x563580)
#define CC_MSGS_ADDR_END            ((char *)0x563668)

// Graphical effects array
// - CC_FX_ARRAY_END-12 is 8 bytes of zeros
// - CC_FX_ARRAY_END-4 is an int that counts down when an effect is active
#define CC_FX_ARRAY_START           ((char *)0x61E170)
#define CC_FX_ARRAY_END             ((char *)0x67BD7C)

// Projectile effects array
// - CC_FX2_ARRAY_START-4 is a timer (don't need to rewind?)
// - CC_FX2_ARRAY_START is a flag that indicates if an effect is active
#define CC_FX2_ARRAY_START          ((char *)0x67BDE8)
#define CC_FX2_ARRAY_END            ((char *)0x746048)

#define CC_OUTRO_FX_ARRAY_START     ((char *)0x74D99C)
#define CC_OUTRO_FX_ARRAY_END       ((char *)0x74DA00)

#define CC_INTRO_FX_ARRAY_START     ((char *)0x74E4C8)
#define CC_INTRO_FX_ARRAY_END       ((char *)0x74E86C)

#define CC_INTRO_FX2_ARRAY_START    ((char *)0x76E6F4)
#define CC_INTRO_FX2_ARRAY_END      ((char *)0x76E7CC)

#define CC_P1_SPELL_CIRCLE_ADDR     ( ( float * )    0x5641A4 )
#define CC_P2_SPELL_CIRCLE_ADDR     ( ( float * )    0x564200 )

#define CC_HIT_SPARKS_ADDR          ( ( uint32_t * ) 0x67BD78 )
#define CC_METER_ANIMATION_ADDR     ( ( uint32_t * ) 0x7717D8 )

#define CC_EFFECTS_ARRAY_ADDR       ( ( char * )     0x67BDE8 )
#define CC_EFFECTS_ARRAY_COUNT      ( 1000 )
#define CC_EFFECT_ELEMENT_SIZE      ( 0x33C )


static const vector<MemDump> playerAddrs =
{
    { 0x555140, 0x555160 },
    { 0x555160, 0x555180 }, // ???
    { 0x555180, 0x555188 },
    { 0x555188, 0x555190 }, // ???
    { 0x555190, 0x555240 },
    ( uint32_t * ) 0x555240, // ???
    { 0x555244, 0x555284 },
    ( uint32_t * ) 0x555284, // ???
    { 0x555288, 0x5552EC },
    ( uint32_t * ) 0x5552EC, // ???
    { 0x5552F0, 0x5552F4 },
    { 0x5552F4, 0x555310 }, // ???
    { 0x555310, 0x55532C },

    { ( void * ) 0x55532C, 4, {
        MemDumpPtr ( 0, 0x24, 1 ),
        MemDumpPtr ( 0, 0x30, 2 )
    } },

    { 0x555330, 0x55534C }, // ???
    { 0x55534C, 0x55535C },
    { 0x55535C, 0x5553CC }, // ???

    { ( void * ) 0x5553CC, 4 }, // pointer to player struct?

    { 0x5553D0, 0x5553EC }, // ???

    { ( void * ) 0x5553EC, 4 }, // pointer to player struct?
    { ( void * ) 0x5553F0, 4 }, // pointer to player struct?

    { 0x5553F4, 0x5553FC },

    { ( void * ) 0x5553FC, 4 }, // pointer to player struct?
    { ( void * ) 0x555400, 4 }, // pointer to player struct?

    { 0x555404, 0x555410 }, // ???
    { 0x555410, 0x55542C },
    ( uint32_t * ) 0x55542C, // ???
    { 0x555430, 0x55544C },

    // { ( void * ) 0x55544C, 4 }, // graphics pointer? this is accessed all the time even when paused
    // { ( void * ) 0x555450, 4 }, // graphics pointer? this is accessed all the time even when paused
    // { ( void * ) 0x555454, 4 }, // graphics pointer? this is accessed all the time even when paused

    { ( void * ) 0x555458, 4 }, // pointer to player struct?

    { 0x55545C, 0x555460 },

    // graphics pointer(s)? these are accessed all the time even when paused
    // { ( void * ) 0x555460, 4, {
    //     MemDumpPtr ( 0, 0x0, 4, {
    //         MemDumpPtr ( 0, 0x4, 4, {
    //             MemDumpPtr ( 0, 0xC, 4 )
    //         } )
    //     } )
    // } },

    { 0x555464, 0x55546C },

    // { ( void * ) 0x55546C, 4 }, // graphics pointer? this is accessed all the time even when paused

    { 0x555470, 0x55550C },
    ( uint32_t * ) 0x55550C, // ???
    { 0x555510, 0x555518 },

    { 0x555518, 0x55561A }, // input history (directions)
    { 0x55561A, 0x55571C }, // input history (A button)
    { 0x55571C, 0x55581E }, // input history (B button)
    { 0x55581E, 0x555920 }, // input history (C button)
    { 0x555920, 0x555A22 }, // input history (D button)
    { 0x555A22, 0x555B24 }, // input history (E button)

    { 0x555B24, 0x555B2C },
    { 0x555B2C, 0x555C30 }, // ???
};

static const vector<MemDump> miscAddrs =
{
    CC_P1_SPELL_CIRCLE_ADDR,
    CC_P2_SPELL_CIRCLE_ADDR,
    CC_HIT_SPARKS_ADDR,
    CC_ROUND_TIMER_ADDR,
    CC_REAL_TIMER_ADDR,
    CC_CAMERA_X_ADDR,
    CC_CAMERA_Y_ADDR,
    CC_METER_ANIMATION_ADDR,
    CC_WORLD_TIMER_ADDR,
    CC_DEATH_TIMER_ADDR,
    CC_INTRO_STATE_ADDR,

    CC_RNGSTATE0_ADDR,
    CC_RNGSTATE1_ADDR,
    CC_RNGSTATE2_ADDR,
    { CC_RNGSTATE3_ADDR, CC_RNGSTATE3_SIZE },

    { CC_P1_SUPER_TIMER1_ADDR, 8 },
    { CC_P1_SUPER_TIMER2_ADDR, 8 },
    { CC_P1_SUPER_TIMER3_ADDR, 8 },
    { CC_P1_SUPER_TIMER4_ADDR, 4 },
    { CC_P1_SUPER_TIMER5_ADDR, 4 },
    { CC_P2_SUPER_TIMER1_ADDR, 8 },
    { CC_P2_SUPER_TIMER2_ADDR, 8 },
    { CC_P2_SUPER_TIMER3_ADDR, 8 },
    { CC_P2_SUPER_TIMER4_ADDR, 4 },
    { CC_P2_SUPER_TIMER5_ADDR, 4 },
};

static const MemDump effectAddrs ( CC_EFFECTS_ARRAY_ADDR, CC_EFFECT_ELEMENT_SIZE, {
    MemDumpPtr ( 0x320, 0x38, 4, {
        MemDumpPtr ( 0, 0, 4, {
            MemDumpPtr ( 0, 0, 4 )
        } )
    } )
} );


int main ( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        PRINT ( "No output file specified!" );
        return -1;
    }

    Logger::get().initialize ( LOG_FILE, 0 );

    MemDumpList allAddrs;

    allAddrs.append ( miscAddrs );

    allAddrs.append ( playerAddrs );                            // Player 1
    allAddrs.append ( playerAddrs, CC_PLR_STRUCT_SIZE );        // Player 2
    allAddrs.append ( playerAddrs, 2 * CC_PLR_STRUCT_SIZE );    // Puppet 1
    allAddrs.append ( playerAddrs, 3 * CC_PLR_STRUCT_SIZE );    // Puppet 2

    for ( size_t i = 0; i < CC_EFFECTS_ARRAY_COUNT; ++i )
        allAddrs.append ( effectAddrs, CC_EFFECT_ELEMENT_SIZE * i );

    allAddrs.update();

    LOG ( "allAddrs.totalSize=%u", allAddrs.totalSize );

    LOG ( "allAddrs:" );
    for ( const MemDump& mem : allAddrs.addrs )
    {
        LOG ( "{ 0x%06X, 0x%06X }", mem.getAddr(), mem.getAddr() + mem.size );

        for ( const MemDumpPtr& ptr0 : mem.ptrs )
        {
            ASSERT ( ptr0.parent == &mem );

            LOG ( "  [0x%x]+0x%x -> { %u bytes }", ptr0.srcOffset, ptr0.dstOffset, ptr0.size );

            for ( const MemDumpPtr& ptr1 : ptr0.ptrs )
            {
                ASSERT ( ptr1.parent == &ptr0 );

                LOG ( "    [0x%x]+0x%x -> { %u bytes }", ptr1.srcOffset, ptr1.dstOffset, ptr1.size );

                for ( const MemDumpPtr& ptr2 : ptr1.ptrs )
                {
                    ASSERT ( ptr2.parent == &ptr1 );

                    LOG ( "      [0x%x]+0x%x -> { %u bytes }", ptr2.srcOffset, ptr2.dstOffset, ptr2.size );
                }
            }
        }
    }

    allAddrs.save ( argv[1] );

    Logger::get().deinitialize();
    return 0;
}
