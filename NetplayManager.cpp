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
    bool hasUpDown = false;

    for ( size_t i = 0; i < 10; ++i )
    {
        if ( i > getFrame() )
            break;

        uint16_t input = getDelayedInput ( player, getFrame() - i );

        if ( ( input & 2 ) || ( input & 8 ) )
        {
            hasUpDown = true;
            break;
        }
    }

    uint16_t input = getDelayedInput ( player );

    // Don't allow pressing select until 10f after we have stopped moving the cursor. This is a work around
    // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
    if ( hasUpDown )
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
    if ( frame < setup.getDelay() )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    return inputs[player - 1].get ( { getIndex(), frame - setup.getDelay() } );
}

bool NetplayManager::areInputsReady() const
{
    if ( state.value < NetplayState::CharaSelect )
        return true;

    return ( inputs[remotePlayer - 1].getEndIndexedFrame().value > indexedFrame.value + 1 + setup.getDelay() );
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

        // Clear old input data
        if ( this->state == NetplayState::Loading )
        {
            // TODO save PerGameData

            inputs[0].clear ( lastStartingIndex, getIndex() );
            inputs[1].clear ( lastStartingIndex, getIndex() );
            lastStartingIndex = getIndex();
        }
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
            // If the remote inputs index is ahead, then we should mash to skip
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
