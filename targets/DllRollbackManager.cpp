#include "DllRollbackManager.hpp"
#include "MemDump.hpp"
#include "DllAsmHacks.hpp"
#include "ErrorStringsExt.hpp"

#include <utility>
#include <algorithm>

using namespace std;


// Linked rollback memory data (binary message format)
extern const unsigned char binary_res_rollback_bin_start;
extern const unsigned char binary_res_rollback_bin_end;

// Deserialized rollback memory data
static MemDumpList allAddrs;

template<typename T>
static inline void deleteArray ( T *ptr ) { delete[] ptr; }


void DllRollbackManager::GameState::save()
{
    ASSERT ( rawBytes != 0 );

    char *dump = rawBytes;

    for ( const MemDump& mem : allAddrs.addrs )
        mem.saveDump ( dump );

    ASSERT ( dump == rawBytes + allAddrs.totalSize );
}

void DllRollbackManager::GameState::load()
{
    ASSERT ( rawBytes != 0 );

    const char *dump = rawBytes;

    for ( const MemDump& mem : allAddrs.addrs )
        mem.loadDump ( dump );

    ASSERT ( dump == rawBytes + allAddrs.totalSize );
}

void DllRollbackManager::allocateStates()
{
    if ( allAddrs.empty() )
    {
        const size_t size = ( ( char * ) &binary_res_rollback_bin_end ) - ( char * ) &binary_res_rollback_bin_start;
        allAddrs.load ( ( char * ) &binary_res_rollback_bin_start, size );
    }

    if ( allAddrs.empty() )
        THROW_EXCEPTION ( "Failed to load rollback data!", ERROR_BAD_ROLLBACK_DATA );

    if ( ! memoryPool )
        memoryPool.reset ( new char[NUM_ROLLBACK_STATES * allAddrs.totalSize], deleteArray<char> );

    for ( size_t i = 0; i < NUM_ROLLBACK_STATES; ++i )
        freeStack.push ( i * allAddrs.totalSize );

    statesList.clear();

    for ( auto& sfxArray : sfxHistory )
        memset ( &sfxArray[0], 0, CC_SFX_ARRAY_LEN );
}

void DllRollbackManager::deallocateStates()
{
    memoryPool.reset();

    while ( ! freeStack.empty() )
        freeStack.pop();

    statesList.clear();
}

void DllRollbackManager::saveState ( const NetplayManager& netMan )
{
    if ( freeStack.empty() )
    {
        ASSERT ( statesList.empty() == false );

        if ( statesList.front().indexedFrame.parts.frame <= netMan.getRemoteFrame() )
        {
            auto it = statesList.begin();
            ++it;
            freeStack.push ( it->rawBytes - memoryPool.get() );
            statesList.erase ( it );
        }
        else
        {
            freeStack.push ( statesList.front().rawBytes - memoryPool.get() );
            statesList.pop_front();
        }
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

    uint8_t *currentSfxArray = &sfxHistory [ netMan.getFrame() % NUM_ROLLBACK_STATES ][0];
    memcpy ( currentSfxArray, AsmHacks::sfxFilterArray, CC_SFX_ARRAY_LEN );
}

bool DllRollbackManager::loadState ( IndexedFrame indexedFrame, NetplayManager& netMan )
{
    if ( statesList.empty() )
    {
        LOG ( "Failed to load state: indexedFrame=%s", indexedFrame );
        return false;
    }

    LOG ( "Trying to load state: indexedFrame=%s; statesList={ %s ... %s }",
          indexedFrame, statesList.front().indexedFrame, statesList.back().indexedFrame );

    const uint32_t origFrame = netMan.getFrame();

    for ( auto it = statesList.rbegin(); it != statesList.rend(); ++it )
    {
#ifdef RELEASE
        if ( ( it->indexedFrame.value <= indexedFrame.value ) || ( & ( *it ) == &statesList.front() ) )
#else
        if ( it->indexedFrame.value <= indexedFrame.value )
#endif
        {
            LOG ( "Loaded state: indexedFrame=%s", it->indexedFrame );

            // Overwrite the current game state
            netMan.state = it->netplayState;
            netMan.startWorldTime = it->startWorldTime;
            netMan.indexedFrame = it->indexedFrame;
            it->load();

            // Erase all other states after the current one.
            // Note: it.base() returns 1 after the position of it, but moving forward.
            for ( auto jt = it.base(); jt != statesList.end(); ++jt )
            {
                freeStack.push ( jt->rawBytes - memoryPool.get() );
            }

            statesList.erase ( it.base(), statesList.end() );

            // Initialize the SFX filter by flagging all played SFX flags in the range (R,S),
            // where R is the actual reset frame, and S is the original starting frame.
            // Note: we can skip frame S, because the current SFX filter array is already initialized by frame S.
            for ( uint32_t i = netMan.getFrame() + 1; i < origFrame; ++i )
            {
                for ( uint32_t j = 0; j < CC_SFX_ARRAY_LEN; ++j )
                    AsmHacks::sfxFilterArray[j] |= sfxHistory [ i % NUM_ROLLBACK_STATES ][j];
            }

            // We set the SFX filter flag to 0x80. Since played (but filtered) SFX are incremented,
            // unplayed sound effects in the filter will stay as 0 or 0x80.
            for ( uint32_t j = 0; j < CC_SFX_ARRAY_LEN; ++j )
            {
                if ( AsmHacks::sfxFilterArray[j] )
                    AsmHacks::sfxFilterArray[j] = 0x80;
            }

            return true;
        }
    }

    LOG ( "Failed to load state: indexedFrame=%s", indexedFrame );
    return false;
}

void DllRollbackManager::saveRerunSounds ( uint32_t frame )
{
    uint8_t *currentSfxArray = &sfxHistory [ frame % NUM_ROLLBACK_STATES ][0];

    // Rewrite the sound effects history during re-run
    for ( uint32_t j = 0; j < CC_SFX_ARRAY_LEN; ++j )
    {
        if ( AsmHacks::sfxFilterArray[j] & ~0x80 )
            currentSfxArray[j] = 1;
        else
            currentSfxArray[j] = 0;
    }
}

void DllRollbackManager::finishedRerunSounds()
{
    // Cancel unplayed sound effects after rollback
    for ( uint32_t j = 0; j < CC_SFX_ARRAY_LEN; ++j )
    {
        // Filter flag 0x80 means the SFX didn't play after rollback since the filter didn't get incremented
        if ( AsmHacks::sfxFilterArray[j] == 0x80 )
        {
            // Play the SFX muted to cancel it
            CC_SFX_ARRAY_ADDR[j] = 1;
            AsmHacks::sfxMuteArray[j] = 1;
        }
    }

    // Cleared last played sound effects
    memset ( AsmHacks::sfxFilterArray, 0, CC_SFX_ARRAY_LEN );
}
