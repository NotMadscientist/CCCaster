#include "ProcessManager.h"

#include <utility>
#include <algorithm>

using namespace std;


#define NUM_ROLLBACK_STATES         ( 120 )


#define CC_UNKNOWN_TIMER_ADDR       ((char *)0x55D1CC) // 4 bytes
#define CC_DEATHTIMER_ADDR          ((char *)0x55D208) // 4 bytes, lo 2 bytes is death timer, hi 1 byte is intro state
#define CC_INTROSTATE_ADDR          ((char *)0x55D20B) // 1 bytes, intro state, 2 (intro), 1 (pre-game), 0 (in-game)
#define CC_OUTROSTATE_ADDR          ((char *)0x563948) // 4 bytes, start from 2, goes to 3 (for timeouts?)

#define CC_PLR_ARRAY_ADDR           ((char *)0x555134) // player info
#define CC_PLR_STRUCT_LEN           (2812)
#define CC_PLR_STRUCT_HEADER        (12)               // stuff that doesn't change

#define CC_CAMERA_X_ADDR            ((char *)0x564B14)
#define CC_CAMERA_Y_ADDR            ((char *)0x564B18)

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

#define CC_P1_SPELL_CIRCLE_ADDR     ((char *)0x5641A4) // 4 bytes, float
#define CC_P2_SPELL_CIRCLE_ADDR     ((char *)0x564200) // 4 bytes, float

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

#define CC_METER_ANIMATION_ADDR     ((char *)0x7717D8)


struct MemoryLocations
{
    vector<pair<void *, void *>> addresses;
    size_t totalSize;

    MemoryLocations ( const vector<pair<void *, void *>>& addresses ) : addresses ( addresses ), totalSize ( 0 )
    {
        sort ( this->addresses.begin(), this->addresses.end(), compareFirst );

        // TODO merge contiguous ranges

        auto it = this->addresses.begin();
        auto jt = it;
        for ( ++jt; jt != this->addresses.end(); )
        {
            if ( it->second == jt->first )
            {
                it->second = jt->second;
                this->addresses.erase ( jt );
                jt = it;
                ++jt;
                continue;
            }

            ++it;
            ++jt;
        }

        for ( const auto& pair : this->addresses )
            totalSize += ( size_t ( pair.second ) - size_t ( pair.first ) );
    }

    static bool compareFirst ( const pair<void *, void *>& a, const pair<void *, void *>& b )
    {
        return a.first < b.first;
    }
};

#define ADDR_FOR_EACH_PLAYER(START, END)                                                                \
    { ( char * ) ( START ), ( char * ) ( END ) },                                                       \
    { ( ( char * ) ( START ) ) + CC_PLR_STRUCT_LEN, ( ( char * ) ( END ) ) + CC_PLR_STRUCT_LEN }

static const MemoryLocations memLocs (
{
    ADDR_FOR_EACH_PLAYER ( 0x555140, 0x555144 ), // seq (see framedisplay)
    ADDR_FOR_EACH_PLAYER ( 0x555144, 0x555148 ), // st (see framedisplay)
    ADDR_FOR_EACH_PLAYER ( 0x5551EC, 0x5551F0 ), // health
    ADDR_FOR_EACH_PLAYER ( 0x5551F0, 0x5551F4 ), // red health
    ADDR_FOR_EACH_PLAYER ( 0x555238, 0x55523C ), // x position
    ADDR_FOR_EACH_PLAYER ( 0x55523C, 0x555240 ), // y position
    ADDR_FOR_EACH_PLAYER ( 0x555244, 0x555248 ), // x position (previous)
    ADDR_FOR_EACH_PLAYER ( 0x555248, 0x55524C ), // y position (previous)
    ADDR_FOR_EACH_PLAYER ( 0x55524C, 0x555250 ), // x velocity
    ADDR_FOR_EACH_PLAYER ( 0x555250, 0x555254 ), // y velocity
    ADDR_FOR_EACH_PLAYER ( 0x555254, 0x555256 ), // x accel
    ADDR_FOR_EACH_PLAYER ( 0x555256, 0x555258 ), // y accel

    { CC_CAMERA_XY2_ADDR, CC_CAMERA_XY2_ADDR + 8 },

    { CC_P1_SUPER_TIMER1_ADDR, CC_P1_SUPER_TIMER1_ADDR + 8 },
    { CC_P1_SUPER_TIMER2_ADDR, CC_P1_SUPER_TIMER2_ADDR + 8 },
    { CC_P1_SUPER_TIMER3_ADDR, CC_P1_SUPER_TIMER3_ADDR + 8 },
    { CC_P1_SUPER_TIMER4_ADDR, CC_P1_SUPER_TIMER4_ADDR + 4 },
    { CC_P1_SUPER_TIMER5_ADDR, CC_P1_SUPER_TIMER5_ADDR + 4 },

    { CC_P2_SUPER_TIMER1_ADDR, CC_P2_SUPER_TIMER1_ADDR + 8 },
    { CC_P2_SUPER_TIMER2_ADDR, CC_P2_SUPER_TIMER2_ADDR + 8 },
    { CC_P2_SUPER_TIMER3_ADDR, CC_P2_SUPER_TIMER3_ADDR + 8 },
    { CC_P2_SUPER_TIMER4_ADDR, CC_P2_SUPER_TIMER4_ADDR + 4 },
    { CC_P2_SUPER_TIMER5_ADDR, CC_P2_SUPER_TIMER5_ADDR + 4 },

    { CC_ROUND_TIMER_ADDR, CC_ROUND_TIMER_ADDR + 1 },
    { CC_REAL_TIMER_ADDR, CC_REAL_TIMER_ADDR + 1 },

    { CC_RNGSTATE0_ADDR, CC_RNGSTATE0_ADDR + 1 },
    { CC_RNGSTATE1_ADDR, CC_RNGSTATE1_ADDR + 1 },

    { CC_RNGSTATE2_ADDR, CC_RNGSTATE2_ADDR + 1 },
    { CC_RNGSTATE3_ADDR, CC_RNGSTATE3_ADDR + CC_RNGSTATE3_SIZE },

    { CC_P1_SPELL_CIRCLE_ADDR, CC_P1_SPELL_CIRCLE_ADDR + 4 },
    { CC_P2_SPELL_CIRCLE_ADDR, CC_P2_SPELL_CIRCLE_ADDR + 4 },

    { CC_CAMERA_X_ADDR, CC_CAMERA_Y_ADDR + 4 },

    { CC_METER_ANIMATION_ADDR, CC_METER_ANIMATION_ADDR + 4 },
} );


void ProcessManager::GameState::save()
{
    ASSERT ( rawBytes != 0 );

    size_t pos = 0;

    for ( const auto& pair : memLocs.addresses )
    {
        copy ( ( char * ) pair.first, ( char * ) pair.second, &rawBytes[pos] );
        pos += ( size_t ( pair.second ) - size_t ( pair.first ) );
    }

    ASSERT ( pos == memLocs.totalSize );
}

void ProcessManager::GameState::load()
{
    ASSERT ( rawBytes != 0 );

    size_t pos = 0;

    for ( const auto& pair : memLocs.addresses )
    {
        size_t size = size_t ( pair.second ) - size_t ( pair.first );
        copy ( &rawBytes[pos], &rawBytes[pos + size], ( char * ) pair.first );
        pos += size;
    }

    ASSERT ( pos == memLocs.totalSize );
}

void ProcessManager::allocateStates()
{
    memoryPool.reset ( new char[NUM_ROLLBACK_STATES * memLocs.totalSize], deleteArray<char> );

    for ( size_t i = 0; i < NUM_ROLLBACK_STATES; ++i )
        freeStack.push ( i * memLocs.totalSize );

    statesList.clear();

    static bool first = true;
    if ( !first )
        return;
    first = false;

    LOG ( "memLocs:" );
    for ( const auto& pair : memLocs.addresses )
        LOG ( "{ %08x, %08x }", pair.first, pair.second );
}

void ProcessManager::deallocateStates()
{
    memoryPool.reset();

    while ( !freeStack.empty() )
        freeStack.pop();

    statesList.clear();
}

void ProcessManager::saveState ( const NetplayManager& netMan )
{
    if ( freeStack.empty() )
    {
        ASSERT ( statesList.empty() == false );

        freeStack.push ( statesList.front().rawBytes - memoryPool.get() );
        statesList.pop_front();
    }

    GameState state =
    {
        netMan.state,
        netMan.startWorldTime,
        netMan.indexedFrame,
        memoryPool.get() + freeStack.top()
    };

    freeStack.pop();
    state.save();
    statesList.push_back ( state );
}

bool ProcessManager::loadState ( IndexedFrame indexedFrame, NetplayManager& netMan )
{
    LOG ( "Trying to load state: indexedFrame=%s", indexedFrame );

    for ( auto it = statesList.rbegin(); it != statesList.rend(); ++it )
    {
        if ( it->indexedFrame.value <= indexedFrame.value )
        {
            LOG ( "Loaded state: indexedFrame=%s", it->indexedFrame );

            // Overwrite the current game state
            netMan.state = it->netplayState;
            netMan.startWorldTime = it->startWorldTime;
            netMan.indexedFrame = it->indexedFrame;
            it->load();

            // Erase all other states after the current one.
            // Note it.base() returns 1 after the position of it, but moving forward.
            for ( auto jt = it.base(); jt != statesList.end(); ++jt )
                freeStack.push ( jt->rawBytes - memoryPool.get() );
            statesList.erase ( it.base(), statesList.end() );
            return true;
        }
    }

    LOG ( "Failed to load state: indexedFrame=%s", indexedFrame );
    return false;
}
