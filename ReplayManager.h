#pragma once

#include "Constants.h"
#include "Protocol.h"

#include <string>
#include <vector>


class ReplayManager
{
public:

    struct Inputs
    {
        IndexedFrame indexedFrame;
        uint16_t p1, p2;
    };

private:

    std::vector<uint32_t> modes;

    std::vector<std::string> states;

    std::vector<std::vector<Inputs>> inputs;

    std::vector<MsgPtr> rngStates;

    std::vector<std::vector<IndexedFrame>> rollbacks;

    std::vector<std::vector<std::vector<Inputs>>> reinputs;

    std::vector<MsgPtr> initialStates;

public:

    bool load ( const std::string& replayFile, bool real );

    uint32_t getGameMode ( IndexedFrame indexedFrame );

    const std::string& getStateStr ( IndexedFrame indexedFrame );

    const Inputs& getInputs ( IndexedFrame indexedFrame );

    IndexedFrame getRollbackTarget ( IndexedFrame indexedFrame );

    const std::vector<Inputs>& getReinputs ( IndexedFrame indexedFrame );

    MsgPtr getRngState ( IndexedFrame indexedFrame );

    uint32_t getLastIndex() const;

    uint32_t getLastFrame() const;
};
