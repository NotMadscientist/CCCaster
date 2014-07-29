#pragma once

#include "Protocol.h"

#include <cereal/types/array.hpp>

#include <array>


#define NUM_INPUTS 30


struct ExitGame : public SerializableMessage
{
    inline ExitGame() {}
    MsgType getMsgType() const override;
};

struct InputData : public SerializableMessage
{
    // The frame index, this starts at 0 when a new transition index starts.
    // This corresponds to the index of the latest input (at frame % NUM_INPUTS).
    uint32_t frame;

    // The transition index
    uint16_t index;

    // The sequence of inputs carried by this message
    std::array<uint16_t, NUM_INPUTS> inputs;

    // Basic constructors
    inline InputData() {}
    inline InputData ( uint32_t frame, uint16_t index ) : frame ( frame ), index ( index ) {}

    PROTOCOL_BOILERPLATE ( frame, index, inputs );
};
