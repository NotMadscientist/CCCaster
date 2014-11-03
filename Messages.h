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


struct ErrorMessage : public SerializableSequence
{
    std::string error;

    ErrorMessage ( const std::string& error ) : error ( error ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( ErrorMessage, error )
};


struct ClientMode : public SerializableSequence
{
    ENUM_VALUE ( ClientMode, Host, Client, Spectate, Broadcast, Offline )

    enum { Training = 0x01, GameStarted = 0x02, UdpTunnel = 0x04 };

    uint8_t flags = 0;

    ClientMode ( Enum value, uint8_t flags = 0 ) : value ( value ), flags ( flags ) {}

    bool isHost() const { return ( value == Host ); }
    bool isClient() const { return ( value == Client ); }
    bool isSpectate() const { return ( value == Spectate ); }
    bool isBroadcast() const { return ( value == Broadcast ); }
    bool isOffline() const { return ( value == Offline ); }
    bool isNetplay() const { return ( value == Host || value == Client ); }
    bool isLocal() const { return ( value == Broadcast || value == Offline ); }

    bool isVersus() const { return !isTraining(); }
    bool isTraining() const { return ( flags & Training ); }
    bool isGameStarted() const { return ( flags & GameStarted ); }
    bool isUdpTunnel() const { return ( flags & UdpTunnel ); }

    std::string flagString() const
    {
        std::string str;

        if ( flags & Training )
            str += "Training";

        if ( flags & GameStarted )
            str += std::string ( str.empty() ? "" : ", " ) + "GameStarted";

        if ( flags & UdpTunnel )
            str += std::string ( str.empty() ? "" : ", " ) + "UdpTunnel";

        return str;
    }

    void clear()
    {
        value = Unknown;
        flags = 0;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( ClientMode, value, flags )
};


struct PingStats : public SerializableSequence
{
    Statistics latency;
    uint8_t packetLoss = 0;

    PingStats ( const Statistics& latency, uint8_t packetLoss ) : latency ( latency ), packetLoss ( packetLoss ) {}

    void clear()
    {
        latency.reset();
        packetLoss = 0;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( PingStats, latency, packetLoss )
};


struct VersionConfig : public SerializableSequence
{
    ClientMode mode;
    Version version;

    VersionConfig ( const ClientMode& mode, uint8_t flags = 0 )
        : mode ( mode.value, mode.flags | flags ), version ( LocalVersion ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( VersionConfig, mode, version )
};


struct InitialConfig : public SerializableSequence
{
    ClientMode mode;
    uint16_t dataPort = 0;
    std::string localName, remoteName;

    std::string getConnectMessage ( const std::string& verb ) const
    {
        return toString ( "%s to %s%s", verb, remoteName, ( mode.isTraining() ? " (training mode)" : "" ) );
    }

    std::string getAcceptMessage ( const std::string& verb ) const
    {
        return toString ( "%s %s", remoteName, verb );
    }

    void clear()
    {
        mode.clear();
        dataPort = 0;
        localName.clear();
        remoteName.clear();
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( InitialConfig, mode, dataPort, localName, remoteName )
};


struct NetplayConfig : public SerializableSequence
{
    ClientMode mode;
    uint8_t delay = 0xFF, rollback = 0;
    uint8_t hostPlayer = 0;
    uint16_t broadcastPort = 0;

    std::array<std::string, 2> names;

    uint8_t getOffset() const
    {
        ASSERT ( delay != 0xFF );

        if ( delay < rollback )
            return 0;
        else
            return delay - rollback;
    }

    void setNames ( const std::string& localName, const std::string& remoteName )
    {
        ASSERT ( hostPlayer == 1 || hostPlayer == 2 );

        names[hostPlayer - 1] = localName;
        names[2 - hostPlayer] = remoteName;
    }

    void clear()
    {
        mode.clear();
        rollback = hostPlayer = 0;
        delay = 0xFF;
        broadcastPort = 0;
        names[0].clear();
        names[1].clear();
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( NetplayConfig, mode, delay, rollback, hostPlayer, broadcastPort, names )
};


struct SpectateConfig : public SerializableSequence
{
    ClientMode mode;
    uint8_t delay = 0xFF, rollback = 0;

    std::array<std::string, 2> names;
    std::array<uint32_t, 2> chara = {{ 0, 0 }};
    std::array<char, 2> moon = {{ 0, 0 }};

    SpectateConfig ( const NetplayConfig& netplayConfig )
        : mode ( netplayConfig.mode ), delay ( netplayConfig.delay )
        , rollback ( netplayConfig.rollback ), names ( netplayConfig.names ) {}

    PROTOCOL_MESSAGE_BOILERPLATE ( SpectateConfig, mode, delay, rollback, names, chara, moon )
};


struct ConfirmConfig : public SerializableSequence
{
    EMPTY_MESSAGE_BOILERPLATE ( ConfirmConfig )
};


struct RngState : public SerializableSequence
{
    uint32_t index = 0;

    uint32_t rngState0 = 0, rngState1 = 0, rngState2 = 0;
    std::array<char, CC_RNGSTATE3_SIZE> rngState3;

    RngState ( uint32_t index ) : index ( index ) {}

    std::string dump() const
    {
        return toString ( "index=%u; { ", index )
               + toBase64 ( &rngState0, sizeof ( rngState0 ) ) + " "
               + toBase64 ( &rngState1, sizeof ( rngState1 ) ) + " "
               + toBase64 ( &rngState2, sizeof ( rngState2 ) ) + " "
               + toBase64 ( &rngState3[0], rngState3.size() ) + " }";
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( RngState, index, rngState0, rngState1, rngState2, rngState3 );
};


struct PerGameData : public SerializableSequence
{
    // The game start index, ie when this batch of inputs start
    uint32_t startIndex = 0;

    // Character select data, indexed by player
    std::array<uint32_t, 2> chara = {{ 0, 0 }}, color = {{ 0, 0 }};
    std::array<char, 2> moon = {{ 0, 0 }};

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
    IndexedFrame indexedFrame = {{ 0, 0 }};

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


struct BothInputs : public SerializableSequence, public BaseInputs
{
    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<std::array<uint16_t, NUM_INPUTS>, 2> inputs;

    BothInputs ( IndexedFrame indexedFrame ) { this->indexedFrame = indexedFrame; }

    PROTOCOL_MESSAGE_BOILERPLATE ( BothInputs, indexedFrame.value, inputs )
};
