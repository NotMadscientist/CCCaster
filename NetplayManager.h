#pragma once

#include "Messages.h"

#include <vector>


class NetplayManager
{
public:

    /* State transitions

    Initial -> CharaSelect

    CharaSelect -> Loading

    Loading -> Transition

    Transition -> { InGame, Retry }

    InGame -> { Transition, Menu (training mode only) }

    Retry -> { Loading, CharaSelect }

    Menu -> { InGame, CharaSelect}

    */

    // Main netplay state
    ENUM ( NetplayState,
           Initial     = 0x01,  // Initial starting phase
           CharaSelect = 0x02,  // Character select
           Loading     = 0x04,  // Loading screen
           Transition  = 0x08,  // Transitions (chara intros, rounds, post-game, pre-retry)
           InGame      = 0x10,  // In-game
           Retry       = 0x20,  // Retry menu
           Menu        = 0x40   // Pause menu or training mode menu
         ) netplayState;

    struct Owner
    {
    };

    Owner *owner = 0;

private:

    std::vector<uint16_t> inputs[2];

public:

    uint8_t delay = 0;

    uint16_t getInput ( uint8_t player, uint32_t frame, uint16_t index );
    void setInput ( uint8_t player, uint32_t frame, uint16_t index, uint16_t input );

    MsgPtr getInputs ( uint8_t player, uint32_t frame, uint16_t index );
    void setInputs ( uint8_t player, const PlayerInputs& playerInputs );

    uint16_t getDelayedInput ( uint8_t player, uint32_t frame, uint16_t index );

    uint32_t getEndFrame ( uint8_t player );
};
