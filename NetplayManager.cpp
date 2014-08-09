#include "NetplayManager.h"
#include "Logger.h"

#include <algorithm>

using namespace std;


#define ASSERT_INPUTS_RANGE(START, END, SIZE)               \
    do {                                                    \
        assert ( END > START );                             \
        assert ( END - START <= NUM_INPUTS );               \
        assert ( START < SIZE );                            \
        assert ( END <= SIZE );                             \
    } while ( 0 )


void NetplayManager::setDelay ( uint8_t delay )
{
    // TODO handle rollback delay
    this->delay = delay;
}

uint16_t NetplayManager::getInput ( uint8_t player, uint32_t frame, uint16_t index ) const
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

MsgPtr NetplayManager::getInputs ( uint8_t player, uint32_t frame, uint16_t index ) const
{
    assert ( player == 1 || player == 2 );
    assert ( inputs[player - 1].empty() == false );

    PlayerInputs *playerInputs = new PlayerInputs ( frame, index );

    // End frame is frame + 1
    if ( frame + 1 > inputs[player - 1].size() )
        frame = inputs[player - 1].size() - 1;

    uint32_t startFrame = 0;
    if ( frame + 1 > NUM_INPUTS )
        startFrame = frame + 1 - NUM_INPUTS;

    ASSERT_INPUTS_RANGE ( startFrame, frame + 1, inputs[player - 1].size() );

    copy ( inputs[player - 1].begin() + startFrame, inputs[player - 1].begin() + ( frame + 1 ),
           playerInputs->inputs.begin() );

    return MsgPtr ( playerInputs );
}

void NetplayManager::setInputs ( uint8_t player, const PlayerInputs& playerInputs )
{
    assert ( player == 1 || player == 2 );

    if ( playerInputs.getEndFrame() > inputs[player - 1].size() )
        inputs[player - 1].resize ( playerInputs.getEndFrame(), 0 );

    ASSERT_INPUTS_RANGE ( playerInputs.getStartFrame(), playerInputs.getEndFrame(), inputs[player - 1].size() );

    copy ( playerInputs.inputs.begin(), playerInputs.inputs.begin() + playerInputs.size(),
           inputs[player - 1].begin() + playerInputs.getStartFrame() );
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player, uint32_t frame, uint16_t index ) const
{
    assert ( player == 1 || player == 2 );

    if ( frame < delay )
        return 0;

    if ( frame - delay >= inputs[player - 1].size() )
        return 0;

    return inputs[player - 1][frame - delay];
}

uint32_t NetplayManager::getEndFrame ( uint8_t player ) const
{
    assert ( player == 1 || player == 2 );

    return inputs[player - 1].size();
}
