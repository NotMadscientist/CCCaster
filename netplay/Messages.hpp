#pragma once

#include "Constants.hpp"
#include "Protocol.hpp"
#include "Logger.hpp"
#include "Statistics.hpp"
#include "Version.hpp"
#include "Compression.hpp"
#include "CharacterSelect.hpp"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

#include <array>
#include <cstring>


struct ErrorMessage : public SerializableSequence
{
    std::string error;

    ErrorMessage ( const std::string& error ) : error ( error ) {}

    std::string str() const override { return format ( "ErrorMessage[%s]", error ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( ErrorMessage, error )
};


struct ClientMode : public SerializableSequence
{
    ENUM_BOILERPLATE ( ClientMode, Host, Client, SpectateNetplay, SpectateBroadcast, Broadcast, Offline )

    enum { Training = 0x01, GameStarted = 0x02, UdpTunnel = 0x04, IsWine = 0x08, VersusCPU = 0x10 };

    uint8_t flags = 0;

    ClientMode ( Enum value, uint8_t flags ) : value ( value ), flags ( flags ) {}

    ClientMode ( const ClientMode& other ) : ClientMode ( other.value, other.flags ) {}

    ClientMode& operator= ( const ClientMode& other ) { value = other.value; flags = other.flags; return *this; }

    bool isUnknown() const { return ( value == Unknown ); }
    bool isHost() const { return ( value == Host ); }
    bool isClient() const { return ( value == Client ); }
    bool isSpectateNetplay() const { return ( value == SpectateNetplay ); }
    bool isSpectateBroadcast() const { return ( value == SpectateBroadcast ); }
    bool isSpectate() const { return ( value == SpectateNetplay || value == SpectateBroadcast ); }
    bool isBroadcast() const { return ( value == Broadcast ); }
    bool isOffline() const { return ( value == Offline ); }
    bool isOnline() const { return !isOffline(); }
    bool isNetplay() const { return ( value == Host || value == Client ); }
    bool isLocal() const { return ( value == Broadcast || value == Offline ); }

    bool isVersus() const { return !isTraining(); }
    bool isVersusCPU() const { return ( flags & VersusCPU ) && !isTraining(); }
    bool isTraining() const { return ( flags & Training ); }
    bool isGameStarted() const { return ( flags & GameStarted ); }
    bool isUdpTunnel() const { return ( flags & UdpTunnel ); }
    bool isWine() const { return ( flags & IsWine ); }
    bool isSinglePlayer() const { return ( isNetplay() || isVersusCPU() ); }

    std::string flagString() const
    {
        std::string str;

        if ( flags & Training )
            str += "Training";

        if ( flags & GameStarted )
            str += std::string ( str.empty() ? "" : ", " ) + "GameStarted";

        if ( flags & UdpTunnel )
            str += std::string ( str.empty() ? "" : ", " ) + "UdpTunnel";

        if ( flags & IsWine )
            str += std::string ( str.empty() ? "" : ", " ) + "IsWine";

        if ( flags & VersusCPU )
            str += std::string ( str.empty() ? "" : ", " ) + "VersusCPU";

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
    uint8_t winCount = 2;

    void clear()
    {
        mode.clear();
        dataPort = 0;
        localName.clear();
        remoteName.clear();
        winCount = 2;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( InitialConfig, mode, dataPort, localName, remoteName, winCount )
};


struct NetplayConfig : public SerializableSequence
{
    ClientMode mode;
    uint8_t delay = 0xFF, rollback = 0, rollbackDelay = 0;
    uint8_t winCount = 2;
    uint8_t hostPlayer = 0;
    uint16_t broadcastPort = 0;

    // Player names
    std::array<std::string, 2> names;

    // Session ID
    std::string sessionId;

    // Offline only tournament mode flag (DO NOT SERIALIZE)
    bool tournament = false;

    void setNames ( const std::string& localName, const std::string& remoteName )
    {
        ASSERT ( hostPlayer == 1 || hostPlayer == 2 );

        if ( mode.isHost() )
        {
            names[hostPlayer - 1] = localName;
            names[2 - hostPlayer] = remoteName;
        }
        else
        {
            names[hostPlayer - 1] = remoteName;
            names[2 - hostPlayer] = localName;
        }
    }

    void clear()
    {
        mode.clear();
        delay = 0xFF;
        rollback = rollbackDelay = hostPlayer = 0;
        winCount = 2;
        broadcastPort = 0;
        names[0].clear();
        names[1].clear();
        sessionId.clear();
        tournament = false;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( NetplayConfig, mode, delay, rollback, rollbackDelay,
                                   winCount, hostPlayer, broadcastPort, names, sessionId )
};


typedef const char * ( *CharaNameFunc ) ( uint8_t chara );

struct InitialGameState : public SerializableSequence
{
    IndexedFrame indexedFrame = {{ 0, 0 }};
    uint32_t stage = 0;
    uint8_t netplayState = 0, isTraining = 0;
    std::array<uint8_t, 2> chara = {{ UNKNOWN_POSITION, UNKNOWN_POSITION }};
    std::array<uint8_t, 2> moon = {{ UNKNOWN_POSITION, UNKNOWN_POSITION }}, color = {{ 0, 0 }};

    InitialGameState ( IndexedFrame indexedFrame ) : indexedFrame ( indexedFrame ) {}

    InitialGameState ( IndexedFrame indexedFrame, uint8_t netplayState, bool isTraining );

    std::string formatCharaName ( uint8_t player, CharaNameFunc charaNameFunc ) const
    {
        ASSERT ( player == 1 || player == 2 );
        ASSERT ( chara[0] != UNKNOWN_POSITION );
        ASSERT ( chara[1] != UNKNOWN_POSITION );
        ASSERT ( moon[0] != UNKNOWN_POSITION );
        ASSERT ( moon[1] != UNKNOWN_POSITION );

        const char moonCh = ( moon[player - 1] == 0 ? 'C' : ( moon[player - 1] == 1 ? 'F' : 'H' ) );

        return format ( "%c-%s", moonCh, charaNameFunc ( chara[player - 1] ) );
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( InitialGameState,
                                   indexedFrame.value, stage, netplayState, isTraining, chara, moon, color )
};


struct SpectateConfig : public SerializableSequence
{
    ClientMode mode;
    uint8_t delay = 0xFF, rollback = 0;
    uint8_t winCount = 2;
    uint8_t hostPlayer = 0;

    // Player names
    std::array<std::string, 2> names;

    // Session ID
    std::string sessionId;

    // Initial game state
    InitialGameState initial;

    SpectateConfig ( const NetplayConfig& netplayConfig, uint8_t state )
        : mode ( netplayConfig.mode )
        , delay ( netplayConfig.rollback ? netplayConfig.rollbackDelay : netplayConfig.delay )
        , rollback ( netplayConfig.rollback ), winCount ( netplayConfig.winCount )
        , hostPlayer ( netplayConfig.hostPlayer ), names ( netplayConfig.names )
        , sessionId ( netplayConfig.sessionId ), initial ( { 0, 0 }, state, netplayConfig.mode.isTraining() ) {}

    std::string formatPlayer ( uint8_t player, CharaNameFunc charaNameFunc ) const
    {
        ASSERT ( player == 1 || player == 2 );

        return format ( ( names[player - 1].empty() ? "%s%s" : "%s (%s)" ),
                        names[player - 1], initial.formatCharaName ( player, charaNameFunc ) );
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( SpectateConfig,
                                   mode, delay, rollback, winCount, hostPlayer, names, sessionId, initial )
};


struct ConfirmConfig : public SerializableSequence
{
    EMPTY_MESSAGE_BOILERPLATE ( ConfirmConfig )
};


struct RngState : public SerializableSequence
{
    uint32_t index = 0;

    uint32_t rngState0 = 0, rngState1 = 0, rngState2 = 0;
    std::array<char, CC_RNG_STATE3_SIZE> rngState3;

    RngState ( uint32_t index ) : index ( index ) {}

    std::string str() const override { return format ( "RngState[%u]", index ); }

    std::string dump() const
    {
        return formatAsHex ( &rngState0, sizeof ( rngState0 ) ) + " "
               + formatAsHex ( &rngState1, sizeof ( rngState1 ) ) + " "
               + formatAsHex ( &rngState2, sizeof ( rngState2 ) ) + " "
               + formatAsHex ( &rngState3[0], rngState3.size() );
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( RngState, index, rngState0, rngState1, rngState2, rngState3 )
};


struct SyncHash : public SerializableSequence
{
    IndexedFrame indexedFrame = {{ 0, 0 }};

    char hash[16];

    uint32_t roundTimer = 0, realTimer = 0;

    int cameraX = 0, cameraY = 0;

    struct CharaHash
    {
        uint32_t seq, seqState, health, redHealth, meter, heat;
        float guardBar, guardQuality;
        int32_t x, y;
        uint16_t chara, moon; // 16bit for struct alignment
    };

    std::array<CharaHash, 2> chara;

    SyncHash ( IndexedFrame indexedFrame );

    bool operator== ( const SyncHash& other ) const
    {
        if ( indexedFrame.value != other.indexedFrame.value )
            return false;

        if ( memcmp ( hash, other.hash, sizeof ( hash ) ) )
            return false;

        if ( roundTimer != other.roundTimer )
            return false;

        if ( realTimer != other.realTimer )
            return false;

        if ( cameraX != other.cameraX )
            return false;

        if ( cameraY != other.cameraY )
            return false;

        if ( memcmp ( ( ( char * ) &chara[0] ) + 8, ( ( char * ) &other.chara[0] ) + 8, sizeof ( CharaHash ) - 8 ) )
            return false;

        if ( memcmp ( ( ( char * ) &chara[1] ) + 8, ( ( char * ) &other.chara[1] ) + 8, sizeof ( CharaHash ) - 8 ) )
            return false;

        if ( chara[0].seq != other.chara[0].seq )
            return false;

        if ( chara[1].seq != other.chara[1].seq )
            return false;

        // Special case, since we don't care if seq 0, the neutral sequence, is out of sync
        if ( chara[0].seq != 0 && chara[0].seqState != other.chara[0].seqState )
            return false;

        if ( chara[1].seq != 0 && chara[1].seqState != other.chara[1].seqState )
            return false;

        return true;
    }

    std::string str() const override { return format ( "SyncHash[%s]", indexedFrame ); }

    std::string dump() const
    {
        std::string str = format ( "[%s] %s; roundTimer=%u; realTimer=%u; camera={ %d, %d }",
                                   indexedFrame, formatAsHex ( hash, sizeof ( hash ) ), roundTimer, realTimer,
                                   cameraX, cameraY );

        for ( uint8_t i = 0; i < 2; ++i )
        {
            str += format ( "; P%u: C=%u; M=%u seq=%u; st=%u; hp=%u; rh=%u; gb=%.1f; gq=%.1f; mt=%u; ht=%u; x=%d; y=%d",
                            i + 1, chara[i].chara, chara[i].moon, chara[i].seq, chara[i].seqState, chara[i].health,
                            chara[i].redHealth, chara[i].guardBar, chara[i].guardQuality, chara[i].meter, chara[i].heat,
                            chara[i].x, chara[i].y );
        }

        return str;
    }

    EMPTY_MESSAGE_BOILERPLATE ( SyncHash )

    void save ( cereal::BinaryOutputArchive& ar ) const override
    {
        ar ( indexedFrame.value, hash, roundTimer, realTimer, cameraX, cameraY );
        char buffer [ sizeof ( CharaHash ) ];
        memcpy ( buffer, &chara[0], sizeof ( CharaHash ) );
        ar ( buffer );
        memcpy ( buffer, &chara[1], sizeof ( CharaHash ) );
        ar ( buffer );
    }

    void load ( cereal::BinaryInputArchive& ar ) override
    {
        ar ( indexedFrame.value, hash, roundTimer, realTimer, cameraX, cameraY );
        char buffer [ sizeof ( CharaHash ) ];
        ar ( buffer );
        memcpy ( &chara[0], buffer, sizeof ( CharaHash ) );
        ar ( buffer );
        memcpy ( &chara[1], buffer, sizeof ( CharaHash ) );
    }
};


struct MenuIndex : public SerializableSequence
{
    uint32_t index = 0;

    int8_t menuIndex = -1;

    MenuIndex ( uint32_t index, int8_t menuIndex ) : index ( index ), menuIndex ( menuIndex ) {}

    std::string str() const override { return format ( "MenuIndex[%u,%d]", index, menuIndex ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( MenuIndex, index, menuIndex )
};


struct ChangeConfig : public SerializableSequence
{
    ENUM_BOILERPLATE ( ChangeConfig, Delay, Rollback )

    IndexedFrame indexedFrame = {{ 0, 0 }};

    uint8_t delay = 0xFF, rollback = 0;

    uint8_t getOffset() const
    {
        ASSERT ( delay != 0xFF );

        if ( delay < rollback )
            return 0;
        else
            return delay - rollback;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( ChangeConfig, value, indexedFrame.value, delay, rollback )
};


struct TransitionIndex : public SerializableMessage
{
    uint32_t index = 0;

    TransitionIndex ( uint32_t index ) : index ( index ) {}

    std::string str() const override { return format ( "TransitionIndex[%u]", index ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( TransitionIndex, index )
};


struct BaseInputs
{
    IndexedFrame indexedFrame = {{ 0, 0 }};

    uint32_t getIndex() const { return indexedFrame.parts.index; }
    uint32_t getFrame() const { return indexedFrame.parts.frame; }

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

    std::string str() const override { return format ( "PlayerInputs[%s]", indexedFrame ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( PlayerInputs, indexedFrame.value, inputs )
};


struct BothInputs : public SerializableSequence, public BaseInputs
{
    // Represents the input range [frame - NUM_INPUTS + 1, frame + 1)
    std::array<std::array<uint16_t, NUM_INPUTS>, 2> inputs;

    BothInputs ( IndexedFrame indexedFrame ) { this->indexedFrame = indexedFrame; }

    std::string str() const override { return format ( "BothInputs[%s]", indexedFrame ); }

    PROTOCOL_MESSAGE_BOILERPLATE ( BothInputs, indexedFrame.value, inputs )
};
