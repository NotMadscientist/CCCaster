#pragma once

#include "Messages.h"
#include "InputsContainer.h"

#include <vector>


// 16 bit input container
struct InputsContainer16 : public SerializableSequence, public InputsContainer<uint16_t>
{
    PROTOCOL_MESSAGE_BOILERPLATE ( InputsContainer16, inputs );
};

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

    InGame -> { Skippable, PauseMenu (training / broadcast / offline only) }

    RetryMenu -> { Loading, CharaSelect }

    PauseMenu -> { InGame, CharaSelect }

*/

// Class that manages netplay state and inputs
class NetplayManager
{
    // Netplay state
    NetplayState state;

    // State of the menu navigation input
    mutable int32_t targetMenuState = -1;

    // Target menu index to navigate to
    mutable int32_t targetMenuIndex = -1;

    // Local retry menu selected index
    mutable int32_t localRetryMenuIndex = -1;

    // Remote retry menu selected index
    mutable int32_t remoteRetryMenuIndex = -1;

    // State of the training mode reset
    mutable int32_t trainingResetState = -1;

    // Type of the training mode reset
    mutable int32_t trainingResetType = 0;

    // The starting value of CC_WORLD_TIMER_ADDR, where frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime.
    // This is reset to the current world time whenever the netplay state changes, ie a state transition happens.
    uint32_t startWorldTime = 0;

    // Current transition index, incremented whenever the netplay state changes (after CharaSelect).
    // Current netplay frame, frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime.
    IndexedFrame indexedFrame = {{ 0, 0 }};

    // The starting index of the current game (starting from the loading state)
    uint32_t startIndex = 0;

    // Mapping: player -> index offset -> frame -> input
    std::array<InputsContainer16, 2> inputs;

    // Mapping: index offset -> RngState (can be null)
    std::vector<MsgPtr> rngStates;

    // The local player, ie the one where setInput is called each frame locally
    uint8_t localPlayer = 1;

    // The remote player, ie the one where setInputs gets called for each input message
    uint8_t remotePlayer = 2;

    // Get the input for the specific netplay state
    uint16_t getPreInitialInput ( uint8_t player ) const;
    uint16_t getInitialInput ( uint8_t player ) const;
    uint16_t getCharaSelectInput ( uint8_t player ) const;
    uint16_t getSkippableInput ( uint8_t player ) const;
    uint16_t getInGameInput ( uint8_t player ) const;
    uint16_t getRetryMenuInput ( uint8_t player ) const;
    uint16_t getPauseMenuInput ( uint8_t player ) const;

    // Get the input for the current frame taking into account delay and rollback
    uint16_t getOffsetInput ( uint8_t player ) const { return getOffsetInput ( player, getFrame() ); }
    uint16_t getOffsetInput ( uint8_t player, uint32_t frame ) const;

    // Get the delayed input for the given frame
    uint16_t getDelayedInput ( uint8_t player ) const { return getDelayedInput ( player, getFrame() ); }
    uint16_t getDelayedInput ( uint8_t player, uint32_t frame ) const;

    // Get the input needed to navigate the menu
    uint16_t getMenuNavInput() const;

    // Detect if up or down has been pressed by either player in the last 2 frames
    bool hasUpDownInLast2f() const;

public:

    // Netplay config
    NetplayConfig config;

    // Indicate which player is the remote player
    void setRemotePlayer ( uint8_t player );

    // Update the current netplay frame
    void updateFrame();
    uint32_t getFrame() const { return indexedFrame.parts.frame; }
    uint32_t getIndex() const { return indexedFrame.parts.index; }
    IndexedFrame getIndexedFrame() const { return indexedFrame; }

    // // Get / clear the last changed frame (for rollback)
    // const IndexedFrame& getLastChangedFrame() const { return inputs[remotePlayer - 1].getLastChangedFrame(); }
    // void clearLastChangedFrame() { inputs[remotePlayer - 1].clearLastChangedFrame(); }

    // Check if we are in a rollback state, with 10 frame initial buffer window
    bool isRollbackState() const { return ( config.rollback && getFrame() >= 10 ); }

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

    // True if remote input is ready for the current frame, otherwise the caller should wait for more input
    bool isRemoteInputReady() const;

    // Get / set the RngState for some index
    MsgPtr getRngState() const;
    void setRngState ( const RngState& rngState );

    // True if the RngState is ready for the current frame, otherwise the caller should wait for it
    bool isRngStateReady ( bool shouldSyncRngState ) const;

    // Get / save the data for the last game
    MsgPtr getLastGame() const;
    void saveLastGame();

    // Get / set the retry menu index
    MsgPtr getRetryMenuIndex() const;
    void setRetryMenuIndex ( uint32_t index );

    friend class ProcessManager;
};
