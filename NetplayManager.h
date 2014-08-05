#pragma once

#include "Messages.h"

#include <vector>


class NetplayManager
{
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
