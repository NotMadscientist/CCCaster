#include "DllRollbackManager.h"
#include "MemDump.h"
#include "AsmHacks.h"
#include "ErrorStringsExt.h"

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

    if ( !memoryPool )
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

    while ( !freeStack.empty() )
        freeStack.pop();

    statesList.clear();
}

void DllRollbackManager::saveState ( const NetplayManager& netMan )
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

    uint8_t *currentSfxArray = &sfxHistory [ netMan.getFrame() % NUM_ROLLBACK_STATES ][0];
    memcpy ( currentSfxArray, AsmHacks::sfxFilterArray, CC_SFX_ARRAY_LEN );
}

bool DllRollbackManager::loadState ( IndexedFrame indexedFrame, NetplayManager& netMan )
{
    LOG ( "Trying to load state: indexedFrame=%s; statesList={ %s ... %s }",
          indexedFrame, statesList.front().indexedFrame, statesList.back().indexedFrame );

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
            // Note: it.base() returns 1 after the position of it, but moving forward.
            for ( auto jt = it.base(); jt != statesList.end(); ++jt )
            {
                freeStack.push ( jt->rawBytes - memoryPool.get() );
            }

            statesList.erase ( it.base(), statesList.end() );
            return true;
        }
    }

    LOG ( "Failed to load state: indexedFrame=%s", indexedFrame );
    return false;
}

void DllRollbackManager::rerunSfx ( bool isLastRerunFrame )
{
}
