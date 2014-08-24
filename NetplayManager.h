#pragma once

#include "Messages.h"
#include "InputsContainer.h"

#include <vector>


// PreInitial: The period while we are preparing communication channels
// Initial: The game starting phase
// CharaSelect: Character select
// Loading: Loading screen, distinct from skippable, so we can transition properly
// Skippable: Skippable states (chara intros, rounds, post-game, pre-retry)
// InGame: In-game state
// RetryMenu: Post-game retry menu
// PauseMenu: Pause menu or training mode menu
ENUM ( NetplayState, PreInitial, Initial, CharaSelect, Loading, Skippable, InGame, RetryMenu, PauseMenu );

/* Netplay state transitions

    PreInitial -> Initial -> CharaSelect -> Loading

    Loading -> { Skippable, InGame (training mode) }

    Skippable -> { InGame (versus mode), RetryMenu }

    InGame -> { Skippable, PauseMenu (broadcast / offline only) }

    RetryMenu -> { Loading, CharaSelect }

    PauseMenu -> { InGame, CharaSelect }

*/


class NetplayManager
{
    // Netplay state
    NetplayState state;

    // Indicates the game mode has been selected at the main menu.
    // This is used because currentMenuIndex will no longer point to the main menu after selecting versus.
    // So we use a flag to indicate when the cursor is on the correct main menu index, and then mash through.
    mutable bool gameModeSelected = false;

    // The starting value of CC_WORLD_TIMER_ADDR, where frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime.
    // This is reset to the current world time whenever the netplay state changes, ie a state transition happens.
    uint32_t startWorldTime = 0;

    // Current transition index, incremented whenever the netplay state changes (after CharaSelect).
    // Current netplay frame, frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime.
    IndexedFrame indexedFrame = { { 0, 0 } };

    // Last index we care about
    uint32_t lastStartingIndex = 0;

    // Last changed indexed frame
    IndexedFrame lastChangedFrame = { { UINT_MAX, UINT_MAX } };

    // Mapping: player -> index -> frame -> input
    std::array<InputsContainer<uint16_t>, 2> inputs;

    // Data for previous games, each game starts after the Loading state
    std::vector<MsgPtr> games;

    // The local player, ie the one where setInput is called each frame
    uint8_t localPlayer = 1;

    // The remote player, ie the one where setInputs gets called
    uint8_t remotePlayer = 2;

    // Get the input for the specific netplay state
    uint16_t getPreInitialInput ( uint8_t player ) const;
    uint16_t getInitialInput ( uint8_t player ) const;
    uint16_t getCharaSelectInput ( uint8_t player ) const;
    uint16_t getSkippableInput ( uint8_t player ) const;
    uint16_t getInGameInput ( uint8_t player ) const;
    uint16_t getRetryMenuInput ( uint8_t player ) const;
    uint16_t getPauseMenuInput ( uint8_t player ) const;

    // Get the delayed input for the given frame
    uint16_t getDelayedInput ( uint8_t player ) const { return getDelayedInput ( player, getFrame() ); }
    uint16_t getDelayedInput ( uint8_t player, uint32_t frame ) const;

public:

    // Netplay setup
    NetplaySetup setup;

    // Indicate which player is the remote player
    void setRemotePlayer ( uint8_t player );

    // Update the current netplay frame
    void updateFrame();
    uint32_t getFrame() const { return indexedFrame.parts.frame; }
    uint32_t getIndex() const { return indexedFrame.parts.index; }
    IndexedFrame getIndexedFrame() const { return indexedFrame; }
    IndexedFrame getLastChangedFrame() const { return lastChangedFrame; }

    // Get / set the current netplay state
    NetplayState getState() const { return state; }
    void setState ( NetplayState state );

    // Get / set the input for the current frame given the player
    uint16_t getInput ( uint8_t player ) const;
    void setInput ( uint8_t player, uint16_t input );

    // Get / set batch inputs for the given player
    MsgPtr getInputs ( uint8_t player ) const;
    void setInputs ( uint8_t player, const PlayerInputs& playerInputs );

    // Get / set batch inputs for the both players
    MsgPtr getBothInputs() const;
    void setBothInputs ( const BothInputs& bothInputs );

    // If inputs are ready to be used, if not then keep polling
    bool areInputsReady() const;

    // Get / set the RNG state for the current index
    MsgPtr getRngState() const;
    void setRngState ( const RngState& rngState );

    friend class ProcessManager;
};
