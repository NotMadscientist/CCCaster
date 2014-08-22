#include "NetplayManager.h"
#include "Logger.h"
#include "AsmHacks.h"
#include "ProcessManager.h"

#include <algorithm>

using namespace std;


#define MAX_GAMES_TO_KEEP ( 5 )


#define ASSERT_INPUTS_RANGE(START, END, SIZE)                       \
    do {                                                            \
        ASSERT ( ( END ) > ( START ) );                             \
        ASSERT ( ( END ) - ( START ) <= ( NUM_INPUTS ) );           \
        ASSERT ( ( START ) < ( SIZE ) );                            \
        ASSERT ( ( END ) <= ( SIZE ) );                             \
    } while ( 0 )

#define RETURN_MASH_INPUT(DIRECTION, BUTTONS)                       \
    do {                                                            \
        if ( frame % 2 )                                            \
            return 0;                                               \
        return COMBINE_INPUT ( ( DIRECTION ), ( BUTTONS ) );        \
    } while ( 0 )


uint16_t NetplayManager::getPreInitialInput ( uint8_t player ) const
{
    if ( ( *CC_GAME_MODE_ADDR ) == CC_GAME_MODE_MAIN )
        return 0;

    RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );
}

uint16_t NetplayManager::getInitialInput ( uint8_t player ) const
{
    if ( ( *CC_GAME_MODE_ADDR ) != CC_GAME_MODE_MAIN )
    {
        gameModeSelected = false;
        return getPreInitialInput ( player );
    }

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

    RETURN_MASH_INPUT ( direction, buttons );
}

uint16_t NetplayManager::getCharaSelectInput ( uint8_t player ) const
{
    uint16_t input = getDelayedInput ( player );

    // Prevent exiting character select
    if ( !charaSelectModes[player - 1] || ( *charaSelectModes[player - 1] ) == CC_CHARA_SELECT_CHARA )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_B | CC_BUTTON_CANCEL );

    return input;
}

uint16_t NetplayManager::getSkippableInput ( uint8_t player ) const
{
    return getDelayedInput ( player );
}

uint16_t NetplayManager::getInGameInput ( uint8_t player ) const
{
    uint16_t input = getDelayedInput ( player );

    // Disable pausing in-game
    input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_START );

    return input;
}

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    uint16_t input = getDelayedInput ( player );

    // Disable saving replay or returning to main menu
    if ( currentMenuIndex >= 2 )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    return input;
}

uint16_t NetplayManager::getPauseMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player ) const
{
    if ( frame < setup.delay )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( inputs.empty() == false );
    ASSERT ( frame - setup.delay < inputs[index][player - 1].size() );

    return inputs[index][player - 1][frame - setup.delay];
}

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
        // Count from frame=0 again
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        frame = 0;

        // Increment the index
        if ( this->state != NetplayState::Initial )
            ++index;

        // Resize the inputs
        if ( index >= inputs.size() )
            inputs.resize ( index + 1 );

        // If we're loading a new game
        if ( this->state == NetplayState::Loading )
        {
            // Add new game data
            games.push_back ( MsgPtr ( new PerGameData ( index ) ) );

            // Clear old game data
            if ( int ( games.size() - 1 ) - MAX_GAMES_TO_KEEP >= 0 )
                games [ int ( games.size() - 1 ) - MAX_GAMES_TO_KEEP ].reset();

            // Clear old input data
            uint32_t startIndex = 0;

            if ( games.size() > 1 )
            {
                ASSERT ( games[games.size() - 1].get() != 0 );
                ASSERT ( games[games.size() - 1]->getMsgType() == MsgType::PerGameData );
                startIndex = games[games.size() - 1]->getAs<PerGameData>().startIndex;
            }

            for ( uint32_t i = startIndex; i < index; ++i )
            {
                inputs[i][0].clear();
                inputs[i][1].clear();
            }
        }
    }

    this->state = state;
}

void NetplayManager::setInput ( uint8_t player, uint16_t input )
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( inputs.empty() == false );

    if ( frame >= inputs[index][player - 1].size() )
        inputs[index][player - 1].resize ( frame + 1, 0 );

    inputs[index][player - 1][frame] = input;
}

MsgPtr NetplayManager::getInputs ( uint8_t player ) const
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( inputs.empty() == false );
    ASSERT ( inputs[index][player - 1].empty() == false );
    ASSERT ( frame + 1 <= inputs[index][player - 1].size() );

    PlayerInputs *playerInputs = new PlayerInputs ( frame, inputs.size() - 1 );

    ASSERT_INPUTS_RANGE ( playerInputs->getStartFrame(),
                          playerInputs->getEndFrame(),
                          inputs[index][player - 1].size() );

    copy ( inputs[index][player - 1].begin() + playerInputs->getStartFrame(),
           inputs[index][player - 1].begin() + playerInputs->getEndFrame(),
           playerInputs->inputs.begin() );

    return MsgPtr ( playerInputs );
}

void NetplayManager::setInputs ( uint8_t player, const PlayerInputs& playerInputs )
{
    ASSERT ( player == 1 || player == 2 );

    if ( playerInputs.index >= inputs.size() )
        inputs.resize ( playerInputs.index + 1 );

    if ( playerInputs.getEndFrame() > inputs[playerInputs.index][player - 1].size() )
        inputs[playerInputs.index][player - 1].resize ( playerInputs.getEndFrame(), 0 );

    ASSERT_INPUTS_RANGE ( playerInputs.getStartFrame(),
                          playerInputs.getEndFrame(),
                          inputs[playerInputs.index][player - 1].size() );

    copy ( playerInputs.inputs.begin(),
           playerInputs.inputs.begin() + playerInputs.size(),
           inputs[playerInputs.index][player - 1].begin() + playerInputs.getStartFrame() );
}

MsgPtr NetplayManager::getBothInputs() const
{
    ASSERT ( inputs.empty() == false );
    ASSERT ( inputs[index][0].empty() == false );
    ASSERT ( inputs[index][1].empty() == false );
    ASSERT ( frame + 1 <= inputs[index][0].size() );
    ASSERT ( frame + 1 <= inputs[index][1].size() );

    BothInputs *bothInputs = new BothInputs ( frame, inputs.size() - 1 );

    const uint32_t endFrame = frame + 1;
    uint32_t startFrame = 0;
    if ( endFrame > NUM_INPUTS )
        startFrame = endFrame - NUM_INPUTS;

    ASSERT_INPUTS_RANGE ( startFrame, endFrame, inputs[index][0].size() );
    ASSERT_INPUTS_RANGE ( startFrame, endFrame, inputs[index][1].size() );

    copy ( inputs[index][0].begin() + startFrame, inputs[index][0].begin() + endFrame, bothInputs->inputs[0].begin() );
    copy ( inputs[index][1].begin() + startFrame, inputs[index][1].begin() + endFrame, bothInputs->inputs[1].begin() );

    return MsgPtr ( bothInputs );
}

void NetplayManager::setBothInputs ( const BothInputs& bothInputs )
{
    if ( bothInputs.index >= inputs.size() )
        inputs.resize ( bothInputs.index + 1 );

    if ( bothInputs.getEndFrame() > inputs[bothInputs.index][0].size() )
        inputs[bothInputs.index][0].resize ( bothInputs.getEndFrame(), 0 );

    if ( bothInputs.getEndFrame() > inputs[bothInputs.index][1].size() )
        inputs[bothInputs.index][1].resize ( bothInputs.getEndFrame(), 0 );

    ASSERT_INPUTS_RANGE ( bothInputs.getStartFrame(), bothInputs.getEndFrame(), inputs[bothInputs.index][0].size() );
    ASSERT_INPUTS_RANGE ( bothInputs.getStartFrame(), bothInputs.getEndFrame(), inputs[bothInputs.index][1].size() );

    copy ( bothInputs.inputs[0].begin(), bothInputs.inputs[0].begin() + bothInputs.size(),
           inputs[bothInputs.index][0].begin() + bothInputs.getStartFrame() );
    copy ( bothInputs.inputs[1].begin(), bothInputs.inputs[1].begin() + bothInputs.size(),
           inputs[bothInputs.index][1].begin() + bothInputs.getStartFrame() );
}

uint16_t NetplayManager::getInput ( uint8_t player ) const
{
    ASSERT ( player == 1 || player == 2 );

    // If the inputs array is ahead, then we should mash to skip
    if ( index + 1 < inputs.size() )
        RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    switch ( state.value )
    {
        case NetplayState::PreInitial:
            return getPreInitialInput ( player );

        case NetplayState::Initial:
            return getInitialInput ( player );

        case NetplayState::CharaSelect:
            return getCharaSelectInput ( player );

        case NetplayState::Loading:
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

    // At this point we should have incremented the index and input array already
    ASSERT ( inputs.empty() == false );

    // If the inputs array is ahead, then we can just mash to skip
    if ( index + 1 < inputs.size() )
        return true;

    ASSERT ( index + 1 == inputs.size() );

    return ( inputs[index][0].size() + setup.delay > frame ) && ( inputs[index][1].size() + setup.delay > frame );
}

MsgPtr NetplayManager::getRngState() const
{
    // TODO
    return 0;
}

void NetplayManager::setRngState ( const RngState& rngState )
{
    // TODO
}
