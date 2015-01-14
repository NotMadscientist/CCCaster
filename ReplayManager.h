#pragma once

#include "Constants.h"
#include "Protocol.h"

#include <string>
#include <vector>


class ReplayManager
{

public:

    struct Input
    {
        IndexedFrame indexedFrame;
        uint16_t p1, p2;
    };

    void load ( const std::string& replayFile );

    uint32_t getGameMode ( IndexedFrame indexedFrame );

    std::string getStateStr ( IndexedFrame indexedFrame );

    const Input& getInputs ( IndexedFrame indexedFrame );

    IndexedFrame getRollbackTarget ( IndexedFrame indexedFrame );

    const std::vector<Input>& getReinputs ( IndexedFrame indexedFrame );

    MsgPtr getRngState ( IndexedFrame indexedFrame );
};
