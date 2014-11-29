#include "NetplayManager.h"
#include "AsmHacks.h"
#include "ProcessManager.h"
#include "Exceptions.h"

#include <algorithm>
#include <cmath>

using namespace std;


// Only allow first 3 options (once again, chara select, save replay).
// Prevent saving replays on Wine because MBAA crashes even without us.
#define MAX_RETRY_MENU_INDEX ( ProcessManager::isWine() ? 1 : 2 )


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

    AsmHacks::menuConfirmState = 2;
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

uint16_t NetplayManager::getInitialCharaSelectInput ( uint8_t player ) const
{
    // TODO character selection
    return 0;
}

uint16_t NetplayManager::getCharaSelectInput ( uint8_t player ) const
{
    uint16_t input = getDelayedInput ( player );

    // Prevent exiting character select
    if ( ( * ( player == 1 ? CC_P1_SELECTOR_MODE_ADDR : CC_P2_SELECTOR_MODE_ADDR ) ) == CC_CHARA_SELECT_CHARA )
    {
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_B | CC_BUTTON_CANCEL );
    }

    return input;
}

uint16_t NetplayManager::getSkippableInput ( uint8_t player ) const
{
    // Only allow the A button here
    return ( getDelayedInput ( player ) & COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM ) );
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
        AsmHacks::menuConfirmState = 2;

        // Don't allow pressing select until 2f after we have stopped moving the cursor. This is a work around
        // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
        if ( hasUpDownInLast2f() )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

        // Disable returning to main menu; 16 and 6 are the menu positions for training and versus mode respectively
        if ( AsmHacks::currentMenuIndex == ( config.mode.isTraining() ? 16 : 6 ) )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

        // Disable returning to chara-select in Wine; 15 and 4 are the menu positions for training and versus mode
        if ( config.mode.isWine() && AsmHacks::currentMenuIndex == ( config.mode.isTraining() ? 15 : 4 ) )
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
                *CC_CAMERA_X_ADDR = -26624;
            }
            else if ( trainingResetType == 1 )
            {
                * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = 45056;
                * ( player == 1 ? CC_P2_X_POSITION_ADDR : CC_P1_X_POSITION_ADDR ) = 61440;
                *CC_CAMERA_X_ADDR = 26624;
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

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player ) const
{
    // Ignore remote input
    if ( player != localPlayer )
        return 0;

    // Auto navigate when final retry menu index has been decided
    if ( targetMenuState != -1 && targetMenuIndex != -1 )
        return getMenuNavInput();

    uint16_t input = getDelayedInput ( player );

    // Don't allow pressing select until 2f after we have stopped moving the cursor. This is a work around
    // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
    if ( hasUpDownInLast2f() )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

    // Limit retry menu selectable options
    if ( AsmHacks::currentMenuIndex > MAX_RETRY_MENU_INDEX )
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

    // Check if auto replay save is enabled
    if ( *CC_AUTO_REPLAY_SAVE_ADDR && AsmHacks::autoReplaySaveStatePtr )
    {
        // Prevent mashing through the auto replay save and causing a hang
        if ( *AsmHacks::autoReplaySaveStatePtr == 100 )
            return 0;
    }

    // Allow saving replays; when manual replay save is selected or any replay save menu is open
    if ( AsmHacks::currentMenuIndex == 2 || *CC_GAME_STATE_COUNTER_ADDR > retryMenuGameStateCounter )
    {
        AsmHacks::menuConfirmState = 2;
        return input;
    }

    if ( config.mode.isNetplay() )
    {
        // Special netplay retry menu behaviour, only select final option after both sides have selected
        if ( remoteRetryMenuIndex != -1 && localRetryMenuIndex != -1 )
        {
            targetMenuState = 0;
            targetMenuIndex = max ( localRetryMenuIndex, remoteRetryMenuIndex );
            targetMenuIndex = min ( targetMenuIndex, 1 ); // Just in case...
            input = 0;
        }
        else if ( localRetryMenuIndex != -1 )
        {
            input = 0;
        }
        else if ( AsmHacks::menuConfirmState == 1 )
        {
            localRetryMenuIndex = AsmHacks::currentMenuIndex;
            input = 0;

            LOG ( "localRetryMenuIndex=%d", localRetryMenuIndex );
        }

        // Disable menu confirms
        AsmHacks::menuConfirmState = 0;
    }
    else
    {
        // Allow regular retry menu operation when not netplaying
        AsmHacks::menuConfirmState = 2;
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

    if ( targetMenuState == 0 )                                         // Determined targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d", targetMenuIndex );

        targetMenuState = 1;
    }
    else if ( targetMenuState == 1 )                                    // Move up or down towards targetMenuIndex
    {
        targetMenuState = 2;

        if ( targetMenuIndex != ( int ) AsmHacks::currentMenuIndex )
            return COMBINE_INPUT ( ( targetMenuIndex < ( int ) AsmHacks::currentMenuIndex ? 8 : 2 ), 0 );
    }
    else if ( targetMenuState >= 2 && targetMenuState <= 4 )            // Wait for currentMenuIndex to update
    {
        ++targetMenuState;
    }
    else if ( targetMenuState == 39 )                                   // Mash final menu selection
    {
        AsmHacks::menuConfirmState = 2;
        RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
    }
    else if ( targetMenuIndex != ( int ) AsmHacks::currentMenuIndex )   // Keep navigating
    {
        targetMenuState = 1;
    }
    else                                                                // Reached targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d; currentMenuIndex=%u", targetMenuIndex, AsmHacks::currentMenuIndex );

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

    if ( state.value >= NetplayState::CharaSelect )
    {
        // Increment the index whenever the netplay state changes
        ++indexedFrame.parts.index;

        // Start counting from frame=0 again
        startWorldTime = *CC_WORLD_TIMER_ADDR;
        indexedFrame.parts.frame = 0;

        // Entering InGame
        if ( state == NetplayState::InGame )
        {
            size_t offset = getIndex() - startIndex;

            inputs[0].eraseIndexOlderThan ( offset );
            inputs[1].eraseIndexOlderThan ( offset );

            if ( offset >= rngStates.size() )
                rngStates.clear();
            else
                rngStates.erase ( rngStates.begin(), rngStates.begin() + offset );

            startIndex = getIndex();
        }

        // Entering RetryMenu
        if ( state == NetplayState::RetryMenu )
        {
            localRetryMenuIndex = -1;
            remoteRetryMenuIndex = -1;

            // The actual retry menu is opened at position *CC_GAME_STATE_COUNTER_ADDR + 1
            retryMenuGameStateCounter = *CC_GAME_STATE_COUNTER_ADDR + 1;
        }

        // Exiting RetryMenu
        if ( this->state == NetplayState::RetryMenu )
        {
            AsmHacks::autoReplaySaveStatePtr = 0;
        }

        // Reset state variables
        AsmHacks::currentMenuIndex = 0;
        AsmHacks::menuConfirmState = 0;
        targetMenuState = -1;
        targetMenuIndex = -1;
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

        case NetplayState::InitialCharaSelect:
            return getInitialCharaSelectInput ( player );

        case NetplayState::CharaSelect:
            return getCharaSelectInput ( player );

        case NetplayState::Loading:
            // If spectating or the remote inputs index is ahead, then we should mash to skip.
            if ( config.mode.isSpectate()
                    || ( ( startIndex + inputs[remotePlayer - 1].getEndIndex() ) > getIndex() + 1 ) )
            {
                AsmHacks::menuConfirmState = 2;
                RETURN_MASH_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
            }

        case NetplayState::Skippable:
            return getSkippableInput ( player );

        case NetplayState::InGame:
            return getInGameInput ( player );

        case NetplayState::RetryMenu:
            return getRetryMenuInput ( player );

        default:
            ASSERT_IMPOSSIBLE;
            return 0;
    }
}

uint16_t NetplayManager::getRawInput ( uint8_t player ) const
{
    ASSERT ( player == 1 || player == 2 );

    switch ( state.value )
    {
        case NetplayState::PreInitial:
        case NetplayState::Initial:
        case NetplayState::InitialCharaSelect:
        case NetplayState::Loading:
        case NetplayState::RetryMenu:
        case NetplayState::Skippable:
        case NetplayState::CharaSelect:
            return getDelayedInput ( player );

        case NetplayState::InGame:
            return getOffsetInput ( player );

        default:
            ASSERT_IMPOSSIBLE;
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
    if ( inputs[0].empty ( getIndex() ) || inputs[1].empty ( getIndex() ) )
        return 0;

    uint32_t minFrame = min ( inputs[0].getEndFrame ( getIndex() ), inputs[1].getEndFrame ( getIndex() ) ) - 1;

    BothInputs *bothInputs = new BothInputs ( { minFrame, getIndex() } );

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

MsgPtr NetplayManager::getRngState ( uint32_t index ) const
{
    if ( config.mode.isOffline() )
        return 0;

    LOG ( "[%s]", indexedFrame );

    ASSERT ( index >= startIndex );

    if ( index + 1 > startIndex + rngStates.size() )
        return 0;

    return rngStates[index - startIndex];
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
