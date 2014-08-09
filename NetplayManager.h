#pragma once

#include "Messages.h"

#include <vector>


// PreInitial: The period while we are preparing communication channels
// Initial: The game starting phase
// CharaSelect: Character select
// Loading: Loading screen, distinct from skippable, so we can transition properly
// Skippable: Skippable states (chara intros, rounds, post-game, pre-retry)
// InGame: In-game state
// Retry: Post-game retry menu
// Menu: Pause menu or training mode menu
ENUM ( NetplayState, PreInitial, Initial, CharaSelect, Loading, Skippable, InGame, Retry, Menu );

/* Netplay state transitions

    PreInitial -> Initial -> CharaSelect -> Loading -> Skippable

    Skippable -> { InGame, Retry }

    InGame -> { Skippable, Menu (training mode only) }

    Retry -> { Loading, CharaSelect }

    Menu -> { InGame, CharaSelect }

*/

class NetplayManager
{
    std::vector<uint16_t> inputs[2];

    // Netplay delay
    uint8_t delay = 0;

public:

    // Netplay state
    NetplayState state;


    uint8_t getDelay() const { return delay; }
    void setDelay ( uint8_t delay );

    uint16_t getInput ( uint8_t player, uint32_t frame, uint16_t index ) const;
    void setInput ( uint8_t player, uint32_t frame, uint16_t index, uint16_t input );

    MsgPtr getInputs ( uint8_t player, uint32_t frame, uint16_t index ) const;
    void setInputs ( uint8_t player, const PlayerInputs& playerInputs );

    uint16_t getDelayedInput ( uint8_t player, uint32_t frame, uint16_t index ) const;

    uint32_t getEndFrame ( uint8_t player ) const;
};
