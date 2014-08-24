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
        if ( getFrame() % 2 )                                       \
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
    // Only allow the A button here
    return ( getDelayedInput ( player ) & COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT ) );
}

uint16_t NetplayManager::getInGameInput ( uint8_t player ) const
{
    // Workaround for round start desync, since inputs have effects during a small period after round start.
    if ( getFrame() < 10 )
        return 0;

    uint16_t input = getDelayedInput ( player );

    // Disable pausing in-game
    input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_START );

    return input;
}

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    uint16_t input = getDelayedInput ( player ), previous = 0;
    if ( getFrame() > 0 )
        previous = getDelayedInput ( player, getFrame() - 1 );

    // Don't allow pressing select until after we have stopped moving the cursor. This is a work around for
    // the issue when select is hit 1 frame after the cursor moves, but before currentMenuIndex is updated.
    if ( ( previous & 2 ) || ( previous & 8 ) || ( input & 2 ) || ( input & 8 ) )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    // Disable saving replay or returning to main menu
    if ( currentMenuIndex >= 2 )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    return input;
}

uint16_t NetplayManager::getPauseMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player, uint32_t frame ) const
{
    if ( frame < setup.offset() )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    return inputs[player - 1].get ( { getIndex(), frame - setup.offset() } );
}

void NetplayManager::updateFrame()
{
    indexedFrame.parts.frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime;
}

void NetplayManager::setState ( NetplayState state )
{
    if ( state == this->state )
        return;

    LOG ( "previous=%s; current=%s", this->state, state );

    if ( state.value >= NetplayState::CharaSelect && state.value <= NetplayState::PauseMenu )
    {
        // Count from frame=0 again
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        indexedFrame.parts.frame = 0;

        // Increment the index
        if ( this->state != NetplayState::Initial )
            ++indexedFrame.parts.index;

        // // Resize the inputs
        // if ( getIndex() >= inputs.size() )
        //     inputs.resize ( getIndex() + 1 );

        // if ( setup.rollback && getIndex() >= isRealInput.size() )
        //     isRealInput.resize ( getIndex() + 1 );

        // // If we're loading a new game
        // if ( this->state == NetplayState::Loading )
        // {
        //     // Add new game data
        //     games.push_back ( MsgPtr ( new PerGameData ( getIndex() ) ) );

        //     // Clear old game data
        //     if ( int ( games.size() - 1 ) - MAX_GAMES_TO_KEEP >= 0 )
        //         games [ int ( games.size() - 1 ) - MAX_GAMES_TO_KEEP ].reset();

        //     // Clear old input data
        //     uint32_t startIndex = 0;

        //     if ( games.size() > 1 )
        //     {
        //         ASSERT ( games[games.size() - 1].get() != 0 );
        //         ASSERT ( games[games.size() - 1]->getMsgType() == MsgType::PerGameData );
        //         startIndex = games[games.size() - 1]->getAs<PerGameData>().startIndex;
        //     }

        //     for ( uint32_t i = startIndex; i < getIndex(); ++i )
        //     {
        //         inputs[i][0].clear();
        //         inputs[i][0].shrink_to_fit();
        //         inputs[i][1].clear();
        //         inputs[i][1].shrink_to_fit();

        //         if ( setup.rollback )
        //         {
        //             isRealInput[i].clear();
        //             isRealInput[i].shrink_to_fit();
        //         }
        //     }

        //     lastStartingIndex = getIndex();
        // }
    }

    this->state = state;
}

void NetplayManager::setInput ( uint8_t player, uint16_t input )
{
    ASSERT ( player == 1 || player == 2 );
    inputs[player - 1].set ( indexedFrame, input );
}

MsgPtr NetplayManager::getInputs ( uint8_t player ) const
{
    PlayerInputs *playerInputs = new PlayerInputs ( indexedFrame );

    ASSERT ( player == 1 || player == 2 );
    inputs[player - 1].get ( playerInputs->getStartIndexedFrame(), &playerInputs->inputs[0], playerInputs->size() );

    return MsgPtr ( playerInputs );
}

void NetplayManager::setInputs ( uint8_t player, const PlayerInputs& playerInputs )
{
    if ( playerInputs.getIndex() < lastStartingIndex )
        return;

    ASSERT ( player == 1 || player == 2 );
    inputs[player - 1].set ( playerInputs.getStartIndexedFrame(), &playerInputs.inputs[0], playerInputs.size() );
}

MsgPtr NetplayManager::getBothInputs() const
{
    // TODO
    return 0;
}

void NetplayManager::setBothInputs ( const BothInputs& bothInputs )
{
    // TODO
}

uint16_t NetplayManager::getInput ( uint8_t player ) const
{
    ASSERT ( player == 1 || player == 2 );

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
            // If the inputs array is ahead, then we should mash to skip
            if ( inputs[remotePlayer - 1].getEndIndex() > getIndex() + 1 )
                RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

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

    return ( indexedFrame.value + setup.offset() < inputs[remotePlayer - 1].getEndIndexedFrame().value );
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

void NetplayManager::setRemotePlayer ( uint8_t player )
{
    ASSERT ( player == 1 || player == 2 );

    localPlayer = 3 - player;
    remotePlayer = player;

    inputs[player - 1].fillFakeInputs = ( setup.rollback > 0 );
}
