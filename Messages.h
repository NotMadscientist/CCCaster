#pragma once

#include "Protocol.h"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>

#include <array>
#include <cassert>


#define NUM_INPUTS 30


struct ClientType : public SerializableMessage
{
    ENUM_MESSAGE_BOILERPLATE ( ClientType, Host, Client );
};


struct PlayerInputs : public SerializableMessage
{
    uint32_t frame = 0;
    uint16_t index = 0;

    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<uint16_t, NUM_INPUTS> inputs;

    inline uint32_t getStartFrame() const { return ( frame + 1 < NUM_INPUTS ) ? 0 : frame + 1 - NUM_INPUTS; }
    inline uint32_t getEndFrame() const { return frame + 1; }
    inline size_t size() const { return getEndFrame() - getStartFrame(); }

    inline PlayerInputs() {}
    inline PlayerInputs ( uint32_t frame, uint16_t index ) : frame ( frame ), index ( index ) {}

    PROTOCOL_BOILERPLATE ( frame, index, inputs );
};


struct DoubleInputs : public SerializableMessage
{
    inline uint32_t getFrame() const { return inputs[0].frame; }
    inline void setFrame ( uint32_t frame ) { inputs[0].frame = inputs[1].frame = frame; }

    inline uint32_t getIndex() const { return inputs[0].frame; }
    inline void setIndex ( uint16_t index ) { inputs[0].index = inputs[1].index = index; }

    inline std::array<uint16_t, NUM_INPUTS>& getInputs ( uint8_t player )
    {
        assert ( player == 1 || player == 2 );
        return inputs[player - 1].inputs;
    }

    inline const PlayerInputs& get ( uint8_t player ) const
    {
        assert ( player == 1 || player == 2 );
        return inputs[player - 1];
    }

    inline DoubleInputs() {}
    inline DoubleInputs ( uint32_t frame, uint16_t index )
    {
        inputs[0].frame = inputs[1].frame = frame;
        inputs[0].index = inputs[1].index = index;
    }

    // Protocol methods

    MsgType getMsgType() const override;

    inline void save ( cereal::BinaryOutputArchive& ar ) const override
    {
        ar ( inputs[0].frame, inputs[0].index, inputs[0].inputs, inputs[1].inputs );
    }

    inline void load ( cereal::BinaryInputArchive& ar ) override
    {
        ar ( inputs[0].frame, inputs[0].index, inputs[0].inputs, inputs[1].inputs );
        inputs[1].frame = inputs[0].frame;
        inputs[1].index = inputs[0].index;
    }

private:

    PlayerInputs inputs[2];
};
