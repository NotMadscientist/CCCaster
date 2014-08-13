#pragma once

#include "Messages.h"

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

    PreInitial -> Initial -> CharaSelect -> Loading -> Skippable

    Skippable -> { InGame, RetryMenu }

    InGame -> { Skippable, PauseMenu (offline or training mode only) }

    RetryMenu -> { Loading, CharaSelect }

    PauseMenu -> { InGame, CharaSelect }

*/

class NetplayManager
{
    const NetplaySetup& setup;

    // Netplay state
    NetplayState state;

    // Indicates the game mode has been selected at the main menu.
    // This is used because currentMenuIndex will no longer point to the main menu after selecting versus.
    mutable bool gameModeSelected = false;

    // The starting value of CC_WORLD_TIMER_ADDR, where frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime.
    // This is reset to the current world time whenever the netplay state changes, ie a state transition happens.
    uint32_t startWorldTime = 0;

    // Current netplay frame, frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime
    uint32_t frame = 0;

    // Current transition index, incremented whenever the netplay state changes (after CharaSelect)
    uint16_t index = 0;

    // Mapping: index -> player -> frame -> input
    std::vector<std::array<std::vector<uint16_t>, 2>> inputs;

    // Get the input for the specific netplay state
    uint16_t getPreInitialInput ( uint8_t player ) const;
    uint16_t getInitialInput ( uint8_t player ) const;
    uint16_t getCharaSelectInput ( uint8_t player ) const;
    uint16_t getSkippableInput ( uint8_t player ) const;
    uint16_t getInGameInput ( uint8_t player ) const;
    uint16_t getRetryMenuInput ( uint8_t player ) const;
    uint16_t getPauseMenuInput ( uint8_t player ) const;

    // Get the delayed input for the current frame
    uint16_t getDelayedInput ( uint8_t player ) const;

public:

    // Basic constructor, stores the provided NetplaySetup as a const reference
    NetplayManager ( const NetplaySetup& setup );

    // Update the current netplay frame
    void updateFrame();

    // Get / set the current netplay state
    const NetplayState& getState() const { return state; }
    void setState ( const NetplayState& state );

    // Get / set the input for the current frame given the player
    uint16_t getInput ( uint8_t player ) const;
    void setInput ( uint8_t player, uint16_t input );

    // Get / set batch inputs for the given player
    MsgPtr getInputs ( uint8_t player ) const;
    void setInputs ( uint8_t player, const PlayerInputs& playerInputs );

    // If inputs are ready to be used, if not then keep polling
    bool areInputsReady() const;
};
