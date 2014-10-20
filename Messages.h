#pragma once

#include "Constants.h"
#include "Protocol.h"
#include "Logger.h"
#include "Statistics.h"
#include "Utilities.h"
#include "Version.h"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

#include <array>


struct EndOfMessages : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( EndOfMessages ) };
struct CharaSelectLoaded : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( CharaSelectLoaded ) };
struct ConfirmConfig : public SerializableSequence { EMPTY_MESSAGE_BOILERPLATE ( ConfirmConfig ) };


struct ErrorMessage : public SerializableSequence
{
    std::string error;

    ErrorMessage ( const std::string& error ) : error ( error ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( ErrorMessage, error )
};


struct ClientMode : public SerializableSequence
{
    ENUM_MESSAGE_BOILERPLATE ( ClientMode, Host, Client, Spectate, Broadcast, Offline )
};


struct PingStats : public SerializableSequence
{
    Statistics latency;
    uint8_t packetLoss = 0;

    PingStats ( const Statistics& latency, uint8_t packetLoss ) : latency ( latency ), packetLoss ( packetLoss ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( PingStats, latency, packetLoss )
};


struct ConfigOptions
{
    enum { Training = 0x01, Spectate = 0x02, Broadcast = 0x04, Offline = 0x08 };

    uint8_t flags = 0;

    ConfigOptions ( uint8_t flags = 0 ) : flags ( flags ) {}

    bool isVersus() const { return !isTraining(); }
    bool isTraining() const { return ( flags & Training ); }
    bool isSpectate() const { return ( flags & Spectate ); }
    bool isBroadcast() const { return ( flags & Broadcast ); }
    bool isOffline() const { return ( flags & Offline ); }
    bool isOnline() const { return !isSpectate() && !isBroadcast() && !isOffline(); }
};


struct VersionConfig : public SerializableSequence, public ConfigOptions
{
    Version version = LocalVersion;

    VersionConfig ( uint8_t flags ) : ConfigOptions ( flags ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( VersionConfig, flags, version )
};


struct SpectateConfig : public SerializableSequence, public ConfigOptions
{

    PROTOCOL_MESSAGE_BOILERPLATE ( SpectateConfig, flags )
};


struct InitialConfig : public SerializableSequence, public ConfigOptions
{
    uint16_t dataPort = 0;
    std::string localName, remoteName;

    std::string getConnectMessage ( const std::string& verb ) const
    {
        return toString ( "%s to %s%s", verb, remoteName, ( isTraining() ? " (training mode)" : "" ) );
    }

    std::string getAcceptMessage ( const std::string& verb ) const
    {
        return toString ( "%s %s", remoteName, verb );
    }

    void clear()
    {
        flags = 0;
        dataPort = 0;
        localName.clear();
        remoteName.clear();
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( InitialConfig, flags, dataPort, localName, remoteName )
};


struct NetplayConfig : public SerializableSequence, public ConfigOptions
{
    uint8_t delay = 0xFF, rollback = 0;
    uint8_t hostPlayer = 0;
    uint16_t broadcastPort = 0;

    uint8_t getOffset() const
    {
        if ( delay < rollback )
            return 0;
        else
            return delay - rollback;
    }

    void clear()
    {
        flags = rollback = hostPlayer = broadcastPort = 0;
        delay = 0xFF;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( NetplayConfig, flags, delay, rollback, hostPlayer, broadcastPort )
};


struct RngState : public SerializableSequence
{
    uint32_t rngState0 = 0, rngState1 = 0, rngState2 = 0;
    std::array<char, CC_RNGSTATE3_SIZE> rngState3;

    std::string dump() const
    {
        return toBase64 ( &rngState0, sizeof ( rngState0 ) )
               + " " + toBase64 ( &rngState1, sizeof ( rngState1 ) )
               + " " + toBase64 ( &rngState2, sizeof ( rngState2 ) )
               + " " + toBase64 ( &rngState3[0], rngState3.size() );
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( RngState, rngState0, rngState1, rngState2, rngState3 );
};


struct PerGameData : public SerializableSequence
{
    // The game start index, ie when this batch of inputs start
    uint32_t startIndex = 0;

    // Character select data, indexed by player
    std::array<uint32_t, 2> chara, moon, color;

    // Selected stage
    uint32_t stage = 0;

    // Mapping: index -> RngState
    std::unordered_map<uint32_t, RngState> rngStates;

    // Mapping: index offset -> player -> frame -> input
    std::vector<std::array<std::vector<uint16_t>, 2>> inputs;

    // TODO encapsulate training mode state

    PerGameData ( uint32_t startIndex ) : startIndex ( startIndex ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( PerGameData, startIndex, chara, color, moon, rngStates, inputs );
};


struct BaseInputs
{
    IndexedFrame indexedFrame = { { 0, 0 } };

    uint32_t getIndex() const { return indexedFrame.parts.index; }
    uint32_t getFrame() const { return indexedFrame.parts.frame; }
    IndexedFrame getStartIndexedFrame() const { return { getIndex(), getStartFrame() }; }

    uint32_t getStartFrame() const
    {
        return ( indexedFrame.parts.frame + 1 < NUM_INPUTS ) ? 0 : indexedFrame.parts.frame + 1 - NUM_INPUTS;
    }

    uint32_t getEndFrame() const { return indexedFrame.parts.frame + 1; }

    size_t size() const { return getEndFrame() - getStartFrame(); }
};


struct PlayerInputs : public SerializableMessage, public BaseInputs
{
    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<uint16_t, NUM_INPUTS> inputs;

    PlayerInputs ( IndexedFrame indexedFrame ) { this->indexedFrame = indexedFrame; }

    PROTOCOL_MESSAGE_BOILERPLATE ( PlayerInputs, indexedFrame.value, inputs )
};


struct BothInputs : public SerializableMessage, public BaseInputs
{
    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<std::array<uint16_t, NUM_INPUTS>, 2> inputs;

    BothInputs ( IndexedFrame indexedFrame ) { this->indexedFrame = indexedFrame; }

    PROTOCOL_MESSAGE_BOILERPLATE ( BothInputs, indexedFrame.value, inputs )
};
