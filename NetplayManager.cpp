#include "NetplayManager.h"
#include "Logger.h"
#include "AsmHacks.h"
#include "ProcessManager.h"

#include <algorithm>

using namespace std;


#define ASSERT_INPUTS_RANGE(START, END, SIZE)               \
    do {                                                    \
        assert ( END > START );                             \
        assert ( END - START <= NUM_INPUTS );               \
        assert ( START < SIZE );                            \
        assert ( END <= SIZE );                             \
    } while ( 0 )


uint16_t NetplayManager::getPreInitialInput ( uint8_t player ) const
{
    if ( ( *CC_GAME_MODE_ADDR ) == CC_GAME_MODE_MAIN )
        return 0;

    if ( frame % 2 )
        return 0;

    return ( CC_BUTTON_A | CC_BUTTON_SELECT );
}

uint16_t NetplayManager::getInitialInput ( uint8_t player ) const
{
    if ( ( *CC_GAME_MODE_ADDR ) != CC_GAME_MODE_MAIN )
    {
        gameModeSelected = false;
        return getPreInitialInput ( player );
    }

    if ( frame % 2 )
        return 0;

    uint16_t direction = 0;
    uint16_t buttons = 0;

    if ( setup.training )
    {
        if ( !gameModeSelected )
        {
            // Training mode is the second option on the main menu
            if ( currentMenuIndex == 2 )
                gameModeSelected = true;
            else
                direction = 2;
        }
        else
        {
            buttons = ( CC_BUTTON_A | CC_BUTTON_SELECT );
        }
    }
    else
    {
        if ( !gameModeSelected )
        {
            // Versus mode is the first option on the main menu
            if ( currentMenuIndex == 1 )
                gameModeSelected = true;
            else
                direction = 2;
        }
        else
        {
            buttons = ( CC_BUTTON_A | CC_BUTTON_SELECT );
        }
    }

    return COMBINE_INPUT ( direction, buttons );
}

uint16_t NetplayManager::getCharaSelectInput ( uint8_t player ) const
{
    // TODO
    return getDelayedInput ( player );
}

uint16_t NetplayManager::getLoadingInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getSkippableInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getInGameInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getPauseMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player ) const
{
    if ( frame < setup.delay )
        return 0;

    assert ( frame - setup.delay < inputs[player - 1].size() );

    return inputs[player - 1][frame - setup.delay];
}

NetplayManager::NetplayManager ( const NetplaySetup& setup ) : setup ( setup ) {}

void NetplayManager::updateFrame()
{
    frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime;
}

void NetplayManager::setState ( const NetplayState& state )
{
    if ( state == this->state )
        return;

    LOG ( "previous=%s; current=%s", this->state, state );

    if ( state.value >= NetplayState::CharaSelect && state.value <= NetplayState::PauseMenu )
    {
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        frame = 0;
        ++index;
    }

    this->state = state;
}

void NetplayManager::setInput ( uint8_t player, uint16_t input )
{
    assert ( player == 1 || player == 2 );

    if ( frame >= inputs[player - 1].size() )
        inputs[player - 1].resize ( frame + 1, 0 );

    inputs[player - 1][frame] = input;
}

MsgPtr NetplayManager::getInputs ( uint8_t player ) const
{
    assert ( player == 1 || player == 2 );
    assert ( inputs[player - 1].empty() == false );
    assert ( frame + 1 <= inputs[player - 1].size() );

    PlayerInputs *playerInputs = new PlayerInputs ( frame, index );

    const uint32_t endFrame = frame + 1;
    uint32_t startFrame = 0;
    if ( endFrame > NUM_INPUTS )
        startFrame = endFrame - NUM_INPUTS;

    ASSERT_INPUTS_RANGE ( startFrame, endFrame, inputs[player - 1].size() );

    copy ( inputs[player - 1].begin() + startFrame, inputs[player - 1].begin() + endFrame,
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

uint16_t NetplayManager::getNetplayInput ( uint8_t player ) const
{
    assert ( player == 1 || player == 2 );

    switch ( state.value )
    {
        case NetplayState::PreInitial:
            return getPreInitialInput ( player );

        case NetplayState::Initial:
            return getInitialInput ( player );

        case NetplayState::CharaSelect:
            return getCharaSelectInput ( player );

        case NetplayState::Loading:
            return getLoadingInput ( player );

        case NetplayState::Skippable:
            return getSkippableInput ( player );

        case NetplayState::InGame:
            return getInGameInput ( player );

        case NetplayState::RetryMenu:
            return getRetryMenuInput ( player );

        case NetplayState::PauseMenu:
            return getPauseMenuInput ( player );

        default:
            LOG_AND_THROW_STRING ( "Invalid state %s!", state );
            return 0;
    }
}

bool NetplayManager::areInputsReady() const
{
    if ( state.value < NetplayState::CharaSelect )
        return true;

    return ( inputs[0].size() + setup.delay > frame ) && ( inputs[1].size() + setup.delay > frame );
}
