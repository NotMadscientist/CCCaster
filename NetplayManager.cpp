#include "NetplayManager.h"

#include <algorithm>

using namespace std;


uint16_t NetplayManager::getInput ( uint8_t player, uint32_t frame, uint16_t index )
{
    assert ( player == 1 || player == 2 );

    if ( frame >= inputs[player - 1].size() )
        return 0;

    return inputs[player - 1][frame];
}

void NetplayManager::setInput ( uint8_t player, uint32_t frame, uint16_t index, uint16_t input )
{
    assert ( player == 1 || player == 2 );

    if ( frame >= inputs[player - 1].size() )
        inputs[player - 1].resize ( frame + 1, 0 );

    inputs[player - 1][frame] = input;
}

MsgPtr NetplayManager::getInputs ( uint8_t player, uint32_t frame, uint16_t index )
{
    assert ( player == 1 || player == 2 );
    assert ( inputs[player - 1].empty() == false );

    PlayerInputs *playerInputs = new PlayerInputs ( frame, index );

    if ( frame >= inputs[player - 1].size() )
        frame = inputs[player - 1].size() - 1;

    uint32_t start = 0;
    if ( frame + 1 > NUM_INPUTS )
        start = frame + 1 - NUM_INPUTS;

    assert ( frame + 1 > start );
    assert ( frame + 1 - start <= NUM_INPUTS );

    assert ( start < inputs[player - 1].size() );
    assert ( frame + 1 <= inputs[player - 1].size() );

    copy ( inputs[player - 1].begin() + start, inputs[player - 1].begin() + ( frame + 1 ),
           playerInputs->inputs.begin() );

    return MsgPtr ( playerInputs );
}

void NetplayManager::setInputs ( uint8_t player, const PlayerInputs& playerInputs )
{
    assert ( player == 1 || player == 2 );

    if ( playerInputs.frame >= inputs[player - 1].size() )
        inputs[player - 1].resize ( playerInputs.frame + 1, 0 );

    assert ( playerInputs.getEndFrame() > playerInputs.getStartFrame() );
    assert ( playerInputs.getEndFrame() - playerInputs.getStartFrame() <= NUM_INPUTS );

    assert ( playerInputs.getStartFrame() < inputs[player - 1].size() );
    assert ( playerInputs.getEndFrame() <= inputs[player - 1].size() );

    copy ( playerInputs.inputs.begin(), playerInputs.inputs.end(),
           inputs[player - 1].begin() + playerInputs.getStartFrame() );
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player, uint32_t frame, uint16_t index )
{
    assert ( player == 1 || player == 2 );

    if ( frame < delay )
        return 0;

    if ( frame - delay >= inputs[player - 1].size() )
        return 0;

    return inputs[player - 1][frame - delay];
}
