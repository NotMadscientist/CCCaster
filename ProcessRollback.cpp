#include "ProcessManager.h"
#include "MemDump.h"

#include <utility>
#include <algorithm>

using namespace std;


#define NUM_ROLLBACK_STATES         ( 120 )

static MemDumpList allAddrs;

template<typename T>
static inline void deleteArray ( T *ptr ) { delete[] ptr; }


void ProcessManager::GameState::save()
{
    ASSERT ( rawBytes != 0 );

    char *dump = rawBytes;

    for ( const MemDump& mem : allAddrs.addrs )
        mem.saveDump ( dump );

    ASSERT ( dump == rawBytes + allAddrs.totalSize );
}

void ProcessManager::GameState::load()
{
    ASSERT ( rawBytes != 0 );

    const char *dump = rawBytes;

    for ( const MemDump& mem : allAddrs.addrs )
        mem.loadDump ( dump );

    ASSERT ( dump == rawBytes + allAddrs.totalSize );
}

void ProcessManager::allocateStates ( const string& appDir )
{
    if ( allAddrs.empty() )
        allAddrs.load ( appDir + FOLDER "states.bin" );

    memoryPool.reset ( new char[NUM_ROLLBACK_STATES * allAddrs.totalSize], deleteArray<char> );

    for ( size_t i = 0; i < NUM_ROLLBACK_STATES; ++i )
        freeStack.push ( i * allAddrs.totalSize );

    statesList.clear();
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

            // TODO enable this after rollback is ready
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
