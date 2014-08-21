#pragma once

#include "Constants.h"
#include "Protocol.h"
#include "Logger.h"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

#include <array>


struct EndOfMessages : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( EndOfMessages ) };
struct CharaSelectLoaded : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( CharaSelectLoaded ) };


struct ErrorMessage : public SerializableSequence
{
    std::string error;

    ErrorMessage ( const std::string& error ) : error ( error ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( ErrorMessage, error )
};


struct ClientType : public SerializableSequence
{
    ENUM_MESSAGE_BOILERPLATE ( ClientType, Host, Client, Broadcast, Offline )
};


struct NetplaySetup : public SerializableSequence
{
    uint8_t delay = 0xFF;
    uint8_t training = 0;
    uint8_t hostPlayer = 0;
    uint16_t broadcastPort = 0;

    PROTOCOL_MESSAGE_BOILERPLATE ( NetplaySetup, delay, training, hostPlayer, broadcastPort )
};


struct RngState : public SerializableSequence
{
    uint32_t rngState0, rngState1, rngState2;
    std::array<char, CC_RNGSTATE3_SIZE> rngState3;

    PROTOCOL_MESSAGE_BOILERPLATE ( RngState, rngState0, rngState1, rngState2, rngState3 );
};


struct PlayerInputs : public SerializableMessage
{
    uint32_t frame = 0;
    uint16_t index = 0;

    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<uint16_t, NUM_INPUTS> inputs;

    uint32_t getStartFrame() const { return ( frame + 1 < NUM_INPUTS ) ? 0 : frame + 1 - NUM_INPUTS; }
    uint32_t getEndFrame() const { return frame + 1; }
    size_t size() const { return getEndFrame() - getStartFrame(); }

    PlayerInputs ( uint32_t frame, uint16_t index ) : frame ( frame ), index ( index ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( PlayerInputs, frame, index, inputs )
};


struct BothInputs : public SerializableMessage
{
    uint32_t frame = 0;
    uint16_t index = 0;

    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<uint16_t, NUM_INPUTS> inputs[2];

    uint32_t getStartFrame() const { return ( frame + 1 < NUM_INPUTS ) ? 0 : frame + 1 - NUM_INPUTS; }
    uint32_t getEndFrame() const { return frame + 1; }
    size_t size() const { return getEndFrame() - getStartFrame(); }

    BothInputs ( uint32_t frame, uint16_t index ) : frame ( frame ), index ( index ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( BothInputs, frame, index, inputs[0], inputs[1] )
};
