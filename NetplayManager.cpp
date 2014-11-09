#include "NetplayManager.h"
#include "Logger.h"
#include "AsmHacks.h"
#include "ProcessManager.h"

#include <algorithm>

using namespace std;


#define MAX_GAMES_TO_KEEP ( 5 )


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

    if ( config.mode.isTraining() )
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
    if ( config.mode.isSpectate() )
    {
        // TODO automatically select character
        return 0;
    }

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

    uint16_t input = getOffsetInput ( player );

    // Disable pausing in netplay versus mode
    if ( config.mode.isNetplay() && config.mode.isVersus() )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_START );

    return input;
}

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    bool hasUpDownInLast10f = false;

    for ( size_t i = 0; i < 10; ++i )
    {
        if ( i > getFrame() )
            break;

        uint16_t input = getDelayedInput ( player, getFrame() - i );

        if ( ( input & 2 ) || ( input & 8 ) )
        {
            hasUpDownInLast10f = true;
            break;
        }
    }

    uint16_t input = getDelayedInput ( player );

    // Don't allow pressing select until 10f after we have stopped moving the cursor. This is a work around
    // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
    if ( hasUpDownInLast10f )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    // Disable saving replay or returning to main menu, ie only allow first two options (once again and chara select)
    // TODO handle replay saving somehow
    if ( currentMenuIndex >= 2 )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );

    return input;
}

uint16_t NetplayManager::getPauseMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getOffsetInput ( uint8_t player ) const
{
    if ( getFrame() < config.getOffset() )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    return inputs[player - 1].get ( getIndex() - startIndex, getFrame() - config.getOffset() );
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player, uint32_t frame ) const
{
    if ( frame < config.delay )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    return inputs[player - 1].get ( getIndex() - startIndex, frame - config.delay );
}

void NetplayManager::setRemotePlayer ( uint8_t player )
{
    ASSERT ( player == 1 || player == 2 );

    localPlayer = 3 - player;
    remotePlayer = player;

    inputs[player - 1].fillFakeInputs = ( config.rollback > 0 );
}

void NetplayManager::updateFrame()
{
    indexedFrame.parts.frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime;
}

void NetplayManager::setState ( NetplayState state )
{
    if ( state == this->state )
        return;

    LOG ( "indexedFrame=[%s]; previous=%s; current=%s", indexedFrame, this->state, state );

    if ( state.value >= NetplayState::CharaSelect && state.value <= NetplayState::PauseMenu )
    {
        // Start counting from frame=0 again
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        indexedFrame.parts.frame = 0;

        // Increment the index
        if ( this->state != NetplayState::Initial )
            ++indexedFrame.parts.index;

        // // Start of a new game, entering loading state
        // if ( state == NetplayState::Loading && !config.mode.isOffline() )
        // {
        //     LOG ( "Start of a new game" );

        //     // Save character select data
        //     PerGameData *game = new PerGameData ( getIndex() );

        //     for ( uint8_t i = 0; i < 2; ++i )
        //     {
        //         game->chara[i] = * ( i == 0 ? CC_P1_CHARA_SELECTOR_ADDR : CC_P2_CHARA_SELECTOR_ADDR );
        //         game->color[i] = ( char ) * ( i == 0 ? CC_P1_COLOR_SELECTOR_ADDR : CC_P2_COLOR_SELECTOR_ADDR );
        //         game->moon[i] = ( char ) * ( i == 0 ? CC_P1_MOON_SELECTOR_ADDR  : CC_P2_MOON_SELECTOR_ADDR  );
        //         game->moon[i] = ( game->moon[i] == 0 ? 'C' : ( game->moon[i] == 1 ? 'F' : 'H' ) );

        //         LOG ( "P%u: chara=%u; color=%u; moon=%c", i + 1, game->chara[i], game->color[i], game->moon[i] );
        //     }

        //     game->stage = *CC_STAGE_SELECTOR_ADDR;

        //     LOG ( "stage=%u", game->stage );

        //     games.push_back ( MsgPtr ( game ) );

        //     // Clear old game data
        //     if ( games.size() > MAX_GAMES_TO_KEEP )
        //     {
        //         games[games.size() - MAX_GAMES_TO_KEEP - 1].reset();

        //         for ( uint32_t i = 0; i < games.size() - MAX_GAMES_TO_KEEP; ++i )
        //             ASSERT ( games[i].get() == 0 );
        //     }
        // }

        // Start of a new game; entering loading state
        if ( state == NetplayState::Loading )
        {
            LOG ( "Start of a new game" );

            inputs[0].clear();
            inputs[1].clear();

            rngStates.clear();

            startIndex = getIndex();
        }
    }

    this->state = state;
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
            // If spectating or the remote inputs index is ahead, then we should mash to skip.
            if ( config.mode.isSpectate()
                    || ( ( startIndex + inputs[remotePlayer - 1].getEndIndex() ) > getIndex() + 1 ) )
            {
                RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_SELECT );
            }

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

void NetplayManager::setInput ( uint8_t player, uint16_t input )
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    inputs[player - 1].set ( getIndex() - startIndex, getFrame(), input );
}

MsgPtr NetplayManager::getInputs ( uint8_t player ) const
{
    PlayerInputs *playerInputs = new PlayerInputs ( indexedFrame );

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( playerInputs->getIndex() >= startIndex );

    inputs[player - 1].get ( playerInputs->getIndex() - startIndex, playerInputs->getStartFrame(),
                             &playerInputs->inputs[0], playerInputs->size() );

    return MsgPtr ( playerInputs );
}

void NetplayManager::setInputs ( uint8_t player, const PlayerInputs& playerInputs )
{
    // Only keep remote inputs at most 1 transition index old, but at least as new as the startIndex
    if ( playerInputs.getIndex() + 1 < getIndex() || playerInputs.getIndex() < startIndex )
        return;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( playerInputs.getIndex() >= startIndex );

    inputs[player - 1].set ( playerInputs.getIndex() - startIndex, playerInputs.getStartFrame(),
                             &playerInputs.inputs[0], playerInputs.size() );
}

MsgPtr NetplayManager::getBothInputs() const
{
    BothInputs *bothInputs = new BothInputs ( indexedFrame );

    ASSERT ( bothInputs->getIndex() >= startIndex );

    inputs[0].get ( bothInputs->getIndex() - startIndex, bothInputs->getStartFrame(),
                    &bothInputs->inputs[0][0], bothInputs->size() );

    inputs[1].get ( bothInputs->getIndex() - startIndex, bothInputs->getStartFrame(),
                    &bothInputs->inputs[1][0], bothInputs->size() );

    return MsgPtr ( bothInputs );
}

void NetplayManager::setBothInputs ( const BothInputs& bothInputs )
{
    // Only keep remote inputs at most 1 transition index old, but at least as new as the startIndex
    if ( bothInputs.getIndex() + 1 < getIndex() || bothInputs.getIndex() < startIndex )
        return;

    ASSERT ( bothInputs.getIndex() >= startIndex );

    inputs[0].set ( bothInputs.getIndex() - startIndex, bothInputs.getStartFrame(),
                    &bothInputs.inputs[0][0], bothInputs.size() );

    inputs[1].set ( bothInputs.getIndex() - startIndex, bothInputs.getStartFrame(),
                    &bothInputs.inputs[1][0], bothInputs.size() );
}

bool NetplayManager::isRemoteInputReady() const
{
    if ( state.value < NetplayState::CharaSelect )
        return true;

    // if ( isRollbackState() && state == NetplayState::InGame )
    // {
    //     return ( ( inputs[remotePlayer - 1].getEndIndexedFrame ( startIndex ).value + config.rollback )
    //              > indexedFrame.value + 1 );
    // }

    if ( inputs[remotePlayer - 1].empty() )
    {
        LOG ( "No remote inputs (index)" );
        return false;
    }

    ASSERT ( inputs[remotePlayer - 1].getEndIndex() >= 1 );

    if ( startIndex + inputs[remotePlayer - 1].getEndIndex() - 1 < getIndex() )
    {
        LOG ( "remoteIndex=%u < localIndex=%u", startIndex + inputs[remotePlayer - 1].getEndIndex() - 1, getIndex() );
        return false;
    }

    if ( startIndex + inputs[remotePlayer - 1].getEndIndex() - 1 > getIndex() )
        return true;

    if ( inputs[remotePlayer - 1].getEndFrame() == 0 )
    {
        LOG ( "No remote inputs (frame)" );
        return false;
    }

    ASSERT ( inputs[remotePlayer - 1].getEndFrame() >= 1 );

    // This causes spontaneous RngState changes
    if ( ( inputs[remotePlayer - 1].getEndFrame() - 1 + config.delay ) < getFrame() )
    {
        LOG ( "remoteFrame = %u + %u delay = %u < localFrame=%u",
              inputs[remotePlayer - 1].getEndFrame() - 1, config.delay,
              inputs[remotePlayer - 1].getEndFrame() - 1 + config.delay,
              getFrame() );

        return false;
    }

    // // This doesn't cause spontaneous RngState changes
    // if ( inputs[remotePlayer - 1].getEndFrame() - 1 < getFrame() )
    // {
    //     LOG ( "remoteFrame%u < localFrame=%u", inputs[remotePlayer - 1].getEndFrame() - 1, getFrame() );
    //     return false;
    // }

    return true;
}

MsgPtr NetplayManager::getRngState() const
{
    if ( config.mode.isOffline() )
        return 0;

    LOG ( "indexedFrame=[%s]", indexedFrame );

    ASSERT ( getIndex() >= startIndex );

    if ( getIndex() + 1 > startIndex + rngStates.size() )
        return 0;

    return rngStates[getIndex() - startIndex];
}

void NetplayManager::setRngState ( const RngState& rngState )
{
    if ( config.mode.isOffline() || rngState.index == 0 || rngState.index < startIndex )
        return;

    LOG ( "indexedFrame=[%s]", indexedFrame );

    ASSERT ( getIndex() >= startIndex );

    if ( getIndex() + 1 > startIndex + rngStates.size() )
        rngStates.resize ( getIndex() + 1 - startIndex );

    rngStates[getIndex() - startIndex].reset ( new RngState ( rngState ) );
}

bool NetplayManager::isRngStateReady ( bool shouldSetRngState ) const
{
    if ( !shouldSetRngState
            || config.mode.isHost() || config.mode.isBroadcast() || config.mode.isOffline()
            || state.value < NetplayState::CharaSelect )
    {
        return true;
    }

    if ( rngStates.empty() )
    {
        LOG ( "No remote RngStates" );
        return false;
    }

    if ( ( startIndex + rngStates.size() - 1 ) < getIndex() )
    {
        LOG ( "remoteIndex=%u < localIndex=%u", startIndex + rngStates.size() - 1, getIndex() );
        return false;
    }

    return true;
}

MsgPtr NetplayManager::getLastGame() const
{
    // if ( games.empty() )
    //     return 0;

    // return games.back();

    return 0;
}

void NetplayManager::saveLastGame()
{
    // if ( config.mode.isOffline() )
    //     return;

    // ASSERT ( games.back().get() != 0 );
    // ASSERT ( games.back()->getMsgType() == MsgType::PerGameData );

    // LOG ( "indexedFrame=[%s]; startIndex=%u", indexedFrame, startIndex );

    // PerGameData& game = games.back()->getAs<PerGameData>();

    // for ( uint32_t i = startIndex; i < rngStates.size(); ++i )
    // {
    //     if ( !rngStates[i] )
    //         continue;

    //     LOG ( "rngState[%d]", i );

    //     ASSERT ( rngStates[i]->getMsgType() == MsgType::RngState );

    //     game.rngStates[i] = rngStates[i]->getAs<RngState>();
    // }
}
