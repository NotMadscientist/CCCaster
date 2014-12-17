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
    {
        allAddrs.load ( appDir + FOLDER "states.bin" );

        LOG ( "allAddrs.totalSize=%u", allAddrs.totalSize );

        LOG ( "allAddrs:" );
        for ( const MemDump& mem : allAddrs.addrs )
        {
            LOG ( "{ %08x, %08x }", mem.getAddr(), mem.getAddr() + mem.size );

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
    }

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
