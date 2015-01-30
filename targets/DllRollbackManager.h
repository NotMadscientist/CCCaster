#pragma once

#include "NetplayManager.h"
#include "Constants.h"

#include <memory>
#include <stack>
#include <list>
#include <array>


class DllRollbackManager
{
    struct GameState
    {
        // Each game state is uniquely identified by (netplayState, startWorldTime, indexedFrame).
        // They are chronologically ordered by index and then frame.
        NetplayState netplayState;
        uint32_t startWorldTime;
        IndexedFrame indexedFrame;

        // The pointer to the raw bytes in the state pool
        char *rawBytes;

        // Save / load the game state
        void save();
        void load();
    };

    // Memory pool to allocate game states
    std::shared_ptr<char> memoryPool;

    // Unused indices in the memory pool, each game state has the same size
    std::stack<size_t> freeStack;

    // List of saved game states in chronological order
    std::list<GameState> statesList;

    // History of sound effect playbacks
    std::array<std::array<uint8_t, CC_SFX_ARRAY_LEN>, NUM_ROLLBACK_STATES> sfxHistory;

public:

    // Allocate / deallocate memory for saving game states
    void allocateStates();
    void deallocateStates();

    // Save / load current game state
    void saveState ( const NetplayManager& netMan );
    bool loadState ( IndexedFrame indexedFrame, NetplayManager& netMan );

    // Handle sound effects during rollback rerun
    void rerunSfx ( bool isLastRerunFrame );
};
