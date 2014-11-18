#include "NetplayManager.h"
#include "Logger.h"
#include "AsmHacks.h"
#include "ProcessManager.h"

#include <algorithm>
#include <cmath>

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

    menuConfirmState = 2;
    RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
}

uint16_t NetplayManager::getInitialInput ( uint8_t player ) const
{
    if ( ( *CC_GAME_MODE_ADDR ) != CC_GAME_MODE_MAIN )
        return getPreInitialInput ( player );

    // The host player should select the main menu, so that the host controls training mode
    if ( player != config.hostPlayer )
        return 0;

    if ( targetMenuState == -1 && targetMenuIndex == -1 )
    {
        targetMenuState = 0;

        if ( config.mode.isTraining() )
            targetMenuIndex = 2; // Training mode is the 2nd option on the main menu
        else
            targetMenuIndex = 1; // Versus mode is the 1st option on the main menu
    }

    return getMenuNavInput();
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
    return ( getDelayedInput ( player ) & COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM ) );
}

static inline int quadraticScale ( int target, int i, int count )
{
    return ( int ) target * ( -pow ( ( double ( i ) / count ) - 1, 2.0 ) + 1 );
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

    // If the pause menu is up
    if ( * ( config.mode.isTraining() ? CC_TRAINING_PAUSE_ADDR : CC_VERSUS_PAUSE_ADDR ) )
    {
        menuConfirmState = 2;

        // Don't allow pressing select until 2f after we have stopped moving the cursor. This is a work around
        // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
        if ( hasUpDownInLast2f() )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

        // Disable returning to main menu; 16 and 6 are the menu positions for training and versus mode respectively
        if ( currentMenuIndex == ( config.mode.isTraining() ? 16 : 6 ) )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

        // Disable returning to chara-select in Wine; 15 and 4 are the menu positions for training and versus mode
        if ( config.mode.isWine() && currentMenuIndex == ( config.mode.isTraining() ? 15 : 4 ) )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
    }
    else if ( config.mode.isTraining() && config.mode.isOffline() && player == config.hostPlayer
              && *CC_DUMMY_STATUS_ADDR != CC_DUMMY_STATUS_DUMMY
              && *CC_DUMMY_STATUS_ADDR != CC_DUMMY_STATUS_RECORD )
    {
        // Training mode enhancements when not paused
        if ( trainingResetState == -1 && ( input & COMBINE_INPUT ( 0, CC_BUTTON_FN2 ) ) )   // Initial reset input
        {
            trainingResetState = 0;

            if ( ( 0xF & input ) == 4 )
                trainingResetType = -1;
            else if ( ( 0xF & input ) == 6 )
                trainingResetType = 1;
            else if ( ( 0xF & input ) == 2 )
                trainingResetType = 2;
            else
                trainingResetType = 0;

            return COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
        }
        else if ( ( trainingResetState == -2 || trainingResetState >= 0 )
                  && ! ( input & COMBINE_INPUT ( 0, CC_BUTTON_FN2 ) ) )                     // Completed reset
        {
            trainingResetState = -1;
        }
        else if ( trainingResetState > 20 )                                                 // Check for held reset
        {
            // Swap sides if reset button is held for 20f
            if ( trainingResetType != 2 && ( input & COMBINE_INPUT ( 0, CC_BUTTON_FN2 ) ) )
                swap ( *CC_P1_X_POSITION_ADDR , *CC_P2_X_POSITION_ADDR );

            trainingResetState = -2;

            return COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
        }
        else if ( trainingResetState >= 0 )                                                 // Reset in progress
        {
            if ( trainingResetType == -1 )
            {
                * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = -45056;
                * ( player == 1 ? CC_P2_X_POSITION_ADDR : CC_P1_X_POSITION_ADDR ) = -61440;
                *CC_CAMERA_X_ADDR = quadraticScale ( -26624, trainingResetState, 10 );
            }
            else if ( trainingResetType == 1 )
            {
                * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = 45056;
                * ( player == 1 ? CC_P2_X_POSITION_ADDR : CC_P1_X_POSITION_ADDR ) = 61440;
                *CC_CAMERA_X_ADDR = quadraticScale ( 26624, trainingResetState, 10 );
            }
            else if ( trainingResetType == 2 )
            {
                * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = 16384;
                * ( player == 1 ? CC_P2_X_POSITION_ADDR : CC_P1_X_POSITION_ADDR ) = -16384;
            }

            ++trainingResetState;

            return COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
        }
    }

    return input;
}

// TODO check / fix replay saving
// Only allow first 2 options (once again and chara select)
#define MAX_RETRY_MENU_INDEX ( 1 )

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    // Ignore remote input
    if ( player != localPlayer )
        return 0;

    if ( targetMenuState != -1 && targetMenuIndex != -1 )
        return getMenuNavInput();

    uint16_t input = getDelayedInput ( player );

    // Don't allow pressing select until 2f after we have stopped moving the cursor. This is a work around
    // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
    if ( hasUpDownInLast2f() )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

    // Limit retry menu selectable options
    if ( currentMenuIndex > MAX_RETRY_MENU_INDEX )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

    if ( config.mode.isNetplay() )
    {
        // Special netplay retry menu behaviour, only select final option after both sides have selected
        if ( remoteRetryMenuIndex != -1 && localRetryMenuIndex != -1 )
        {
            targetMenuState = 0;
            targetMenuIndex = max ( localRetryMenuIndex, remoteRetryMenuIndex );
            targetMenuIndex = min ( targetMenuIndex, MAX_RETRY_MENU_INDEX ); // Just in case...
            input = 0;
        }
        else if ( localRetryMenuIndex != -1 )
        {
            input = 0;
        }
        else if ( menuConfirmState == 1 )
        {
            localRetryMenuIndex = currentMenuIndex;
            input = 0;

            LOG ( "localRetryMenuIndex=%d", localRetryMenuIndex );
        }
    }

    return input;
}

MsgPtr NetplayManager::getRetryMenuIndex() const
{
    if ( state == NetplayState::RetryMenu && localRetryMenuIndex != -1 )
        return MsgPtr ( new MenuIndex ( localRetryMenuIndex ) );
    else
        return 0;
}

void NetplayManager::setRetryMenuIndex ( uint32_t position )
{
    remoteRetryMenuIndex = position;

    LOG ( "remoteRetryMenuIndex=%d", remoteRetryMenuIndex );
}

uint16_t NetplayManager::getPauseMenuInput ( uint8_t player ) const
{
    return 0;
}

uint16_t NetplayManager::getOffsetInput ( uint8_t player, uint32_t frame ) const
{
    if ( frame < config.getOffset() )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    return inputs[player - 1].get ( getIndex() - startIndex, frame - config.getOffset() );
}

uint16_t NetplayManager::getDelayedInput ( uint8_t player, uint32_t frame ) const
{
    if ( frame < config.delay )
        return 0;

    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    return inputs[player - 1].get ( getIndex() - startIndex, frame - config.delay );
}

uint16_t NetplayManager::getMenuNavInput() const
{
    if ( targetMenuState == -1 || targetMenuIndex == -1 )
        return 0;

    if ( targetMenuState == 0 )                                 // Determined targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d", targetMenuIndex );

        targetMenuState = 1;
    }
    else if ( targetMenuState == 1 )                            // Move up or down towards targetMenuIndex
    {
        targetMenuState = 2;

        if ( targetMenuIndex != ( int ) currentMenuIndex )
            return COMBINE_INPUT ( ( targetMenuIndex < ( int ) currentMenuIndex ? 8 : 2 ), 0 );
    }
    else if ( targetMenuState >= 2 && targetMenuState <= 4 )    // Wait for currentMenuIndex to update
    {
        ++targetMenuState;
    }
    else if ( targetMenuState == 39 )                           // Mash final menu selection
    {
        menuConfirmState = 2;
        RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
    }
    else if ( targetMenuIndex != ( int ) currentMenuIndex )             // Keep navigating
    {
        targetMenuState = 1;
    }
    else                                                        // Reached targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d; currentMenuIndex=%u", targetMenuIndex, currentMenuIndex );

        targetMenuState = 39;
    }

    return 0;
}

bool NetplayManager::hasUpDownInLast2f() const
{
    for ( size_t i = 0; i < 2; ++i )
    {
        if ( i > getFrame() )
            break;

        const uint16_t p1 = 0xF & ( state.value == NetplayState::InGame
                                    ? getOffsetInput ( 1, getFrame() - i )
                                    : getDelayedInput ( 1, getFrame() - i ) );

        const uint16_t p2 = 0xF & ( state.value == NetplayState::InGame
                                    ? getOffsetInput ( 2, getFrame() - i )
                                    : getDelayedInput ( 2, getFrame() - i ) );

        if ( ( p1 == 2 ) || ( p1 == 8 ) || ( p2 == 2 ) || ( p2 == 8 ) )
            return true;
    }

    return false;
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
    ASSERT ( this->state != state );

    LOG ( "indexedFrame=[%s]; previous=%s; current=%s", indexedFrame, this->state, state );

    if ( state.value >= NetplayState::CharaSelect && state.value <= NetplayState::PauseMenu )
    {
        // Increment the index
        ++indexedFrame.parts.index;

        // Start counting from frame=0 again
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        indexedFrame.parts.frame = 0;

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

            targetMenuState = -1;
            targetMenuIndex = -1;
            localRetryMenuIndex = -1;
            remoteRetryMenuIndex = -1;
        }

        // Reset state variables
        currentMenuIndex = 0;
        menuConfirmState = 0;
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
                menuConfirmState = 2;
                RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
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
    if ( state.value < NetplayState::CharaSelect || state.value == NetplayState::RetryMenu )
        return true;

    // if ( isRollbackState() && state == NetplayState::InGame )
    // {
    //     return ( ( inputs[remotePlayer - 1].getEndIndexedFrame ( startIndex ).value + config.rollback )
    //              > indexedFrame.value + 1 );
    // }

    if ( inputs[remotePlayer - 1].empty() )
    {
        LOG ( "[%s] No remote inputs (index)", indexedFrame );
        return false;
    }

    ASSERT ( inputs[remotePlayer - 1].getEndIndex() >= 1 );

    if ( startIndex + inputs[remotePlayer - 1].getEndIndex() - 1 < getIndex() )
    {
        LOG ( "[%s] remoteIndex=%u < localIndex=%u",
              indexedFrame, startIndex + inputs[remotePlayer - 1].getEndIndex() - 1, getIndex() );
        return false;
    }

    // If remote index is ahead, we must be in an older state, so we don't need to wait for inputs
    if ( startIndex + inputs[remotePlayer - 1].getEndIndex() - 1 > getIndex() )
        return true;

    if ( inputs[remotePlayer - 1].getEndFrame() == 0 )
    {
        LOG ( "[%s] No remote inputs (frame)", indexedFrame );
        return false;
    }

    ASSERT ( inputs[remotePlayer - 1].getEndFrame() >= 1 );

    if ( ( inputs[remotePlayer - 1].getEndFrame() - 1 + config.delay ) < getFrame() )
    {
        LOG ( "[%s] remoteFrame = %u + %u delay = %u < localFrame=%u",
              indexedFrame,
              inputs[remotePlayer - 1].getEndFrame() - 1, config.delay,
              inputs[remotePlayer - 1].getEndFrame() - 1 + config.delay,
              getFrame() );

        return false;
    }

    return true;
}

MsgPtr NetplayManager::getRngState() const
{
    if ( config.mode.isOffline() )
        return 0;

    LOG ( "[%s]", indexedFrame );

    ASSERT ( getIndex() >= startIndex );

    if ( getIndex() + 1 > startIndex + rngStates.size() )
        return 0;

    return rngStates[getIndex() - startIndex];
}

void NetplayManager::setRngState ( const RngState& rngState )
{
    if ( config.mode.isOffline() || rngState.index == 0 || rngState.index < startIndex )
        return;

    LOG ( "[%s] rngState.index=%u", indexedFrame, rngState.index );

    ASSERT ( rngState.index >= startIndex );

    if ( rngState.index + 1 > startIndex + rngStates.size() )
        rngStates.resize ( rngState.index + 1 - startIndex );

    rngStates[rngState.index - startIndex].reset ( new RngState ( rngState ) );
}

bool NetplayManager::isRngStateReady ( bool shouldSyncRngState ) const
{
    if ( !shouldSyncRngState
            || config.mode.isHost() || config.mode.isBroadcast() || config.mode.isOffline()
            || state.value < NetplayState::CharaSelect )
    {
        return true;
    }

    if ( rngStates.empty() )
    {
        LOG ( "[%s] No remote RngStates", indexedFrame );
        return false;
    }

    if ( ( startIndex + rngStates.size() - 1 ) < getIndex() )
    {
        LOG ( "[%s] remoteIndex=%u < localIndex=%u", indexedFrame, startIndex + rngStates.size() - 1, getIndex() );
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
