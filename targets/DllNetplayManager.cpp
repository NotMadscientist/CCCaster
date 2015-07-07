#include "DllNetplayManager.hpp"
#include "DllAsmHacks.hpp"
#include "ProcessManager.hpp"
#include "Exceptions.hpp"
#include "CharacterSelect.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

using namespace std;


// Max allowed retry menu index (once again, chara select, save replay).
// Prevent saving replays during rollback for obvious reasons.
// Prevent saving replays on Wine because MBAA crashes even without us.
#define MAX_RETRY_MENU_INDEX ( ( config.rollback || config.mode.isWine() ) ? 1 : 2 )

// Extra number to add to preserveStartIndex, this is a safety buffer for chained spectators.
#define PRESERVE_START_INDEX_BUFFER ( 5 )


#define RETURN_MASH_INPUT(DIRECTION, BUTTONS)                       \
    do {                                                            \
        if ( getFrame() % 2 )                                       \
            return 0;                                               \
        return COMBINE_INPUT ( ( DIRECTION ), ( BUTTONS ) );        \
    } while ( 0 )


uint16_t NetplayManager::getPreInitialInput ( uint8_t player )
{
    if ( ( *CC_GAME_MODE_ADDR ) == CC_GAME_MODE_MAIN )
        return 0;

    AsmHacks::menuConfirmState = 2;
    RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );
}

uint16_t NetplayManager::getInitialInput ( uint8_t player )
{
    if ( ( *CC_GAME_MODE_ADDR ) != CC_GAME_MODE_MAIN )
        return getPreInitialInput ( player );

    // The host player should select the main menu, so that the host controls training mode
    if ( player != config.hostPlayer )
        return 0;

    // Wait until we know what game mode to go to
    if ( config.mode.isUnknown() )
        return 0;

    AsmHacks::menuConfirmState = 2;
    RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );
}

uint16_t NetplayManager::getAutoCharaSelectInput ( uint8_t player )
{
    *CC_P1_CHARA_SELECTOR_ADDR = ( uint32_t ) charaToSelector ( initial.chara[0] );
    *CC_P2_CHARA_SELECTOR_ADDR = ( uint32_t ) charaToSelector ( initial.chara[1] );

    *CC_P1_CHARACTER_ADDR = ( uint32_t ) initial.chara[0];
    *CC_P2_CHARACTER_ADDR = ( uint32_t ) initial.chara[1];

    *CC_P1_MOON_SELECTOR_ADDR = ( uint32_t ) initial.moon[0];
    *CC_P2_MOON_SELECTOR_ADDR = ( uint32_t ) initial.moon[1];

    *CC_P1_COLOR_SELECTOR_ADDR = ( uint32_t ) initial.color[0];
    *CC_P2_COLOR_SELECTOR_ADDR = ( uint32_t ) initial.color[1];

    *CC_STAGE_SELECTOR_ADDR = initial.stage;

    RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );
}

uint16_t NetplayManager::getCharaSelectInput ( uint8_t player )
{
    uint16_t input = getRawInput ( player );

    // Prevent hitting Confirm until 150f after beginning of CharaSelect, this is to workaround the moon selector desync
    if ( config.mode.isOnline() && getFrame() < 150 )
    {
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
    }

    // Prevent exiting character select
    if ( ( * ( player == 1 ? CC_P1_SELECTOR_MODE_ADDR : CC_P2_SELECTOR_MODE_ADDR ) ) == CC_SELECT_CHARA )
    {
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_B | CC_BUTTON_CANCEL );
    }

    // Don't allow hitting Confirm or Cancel until 2f after we have stopped changing the selector mode
    if ( hasButtonInHistory ( player, CC_BUTTON_A | CC_BUTTON_CONFIRM | CC_BUTTON_B | CC_BUTTON_CANCEL, 1, 3 ) )
    {
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM | CC_BUTTON_B | CC_BUTTON_CANCEL );
    }

    return input;
}

uint16_t NetplayManager::getSkippableInput ( uint8_t player )
{
    if ( config.mode.isSpectate() )
        RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );

    // Only allow the confirm and cancel here
    return ( getRawInput ( player ) & COMBINE_INPUT ( 0, CC_BUTTON_CONFIRM | CC_BUTTON_CANCEL ) );
}

uint16_t NetplayManager::getInGameInput ( uint8_t player )
{
    uint16_t input = getRawInput ( player );

    // Disable pausing in netplay versus mode. Also only allow start button in versus after holding it for a duration.
    if ( ( ( config.mode.isNetplay() && config.mode.isVersus() ) || config.mode.isSpectate() )
            || ( ! *CC_PAUSE_FLAG_ADDR
                 && config.mode.isVersus()
                 && heldStartDuration
                 && ! heldButtonInHistory ( player, CC_BUTTON_START, 0, heldStartDuration ) ) )
    {
        input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_START );
    }

    // If the pause menu is up
    if ( *CC_PAUSE_FLAG_ADDR )
    {
        AsmHacks::menuConfirmState = 2;

        // Don't allow hitting Confirm until 3f after we have stopped moving the cursor. This is a work around
        // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
        if ( hasUpDownInHistory ( player, 0, 3 ) )
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

            input |= COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
        }
        else if ( ( trainingResetState == -2 || trainingResetState >= 0 )
                  && ! ( input & COMBINE_INPUT ( 0, CC_BUTTON_FN2 ) ) )                     // Completed reset
        {
            trainingResetState = -1;
        }
        else if ( trainingResetState > 10 )                                                 // Check for held reset
        {
            // Swap sides if reset button is held for 10f
            if ( input & COMBINE_INPUT ( 0, CC_BUTTON_FN2 ) )
            {
                if ( trainingResetType == 0 )
                    swap ( *CC_P1_X_POSITION_ADDR , *CC_P2_X_POSITION_ADDR );
                else if ( trainingResetType == -1 )
                    * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = -65536;
                else if ( trainingResetType == 1 )
                    * ( player == 1 ? CC_P1_X_POSITION_ADDR : CC_P2_X_POSITION_ADDR ) = 65536;

                if ( trainingResetType != 2 )
                    swap ( *CC_P1_FACING_FLAG_ADDR , *CC_P2_FACING_FLAG_ADDR );
            }

            trainingResetState = -2;

            input |= COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
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

            input |= COMBINE_INPUT ( 0, CC_BUTTON_FN2 );
        }
    }

    return input;
}

uint16_t NetplayManager::getRetryMenuInput ( uint8_t player )
{
    // Ignore remote input on netplay
    if ( player != localPlayer && config.mode.isNetplay() )
        return 0;

    // Auto navigate when final retry menu index has been decided
    if ( targetMenuState != -1 && targetMenuIndex != -1 )
        return getMenuNavInput();

    uint16_t input;

    if ( config.mode.isSpectateNetplay() )
    {
        input = ( ( getFrame() % 2 ) ? 0 : COMBINE_INPUT ( 0, CC_BUTTON_CONFIRM ) );
    }
    else
    {
        input = getRawInput ( player );

        // Don't allow hitting Confirm until 3f after we have stopped moving the cursor. This is a work around
        // for the issue when select is pressed after the cursor moves, but before currentMenuIndex is updated.
        if ( hasUpDownInHistory ( config.mode.isNetplay() ? player : 0, 0, 3 ) )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );

        // Limit retry menu selectable options
        if ( AsmHacks::currentMenuIndex > MAX_RETRY_MENU_INDEX )
            input &= ~ COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM );
    }

    // Check if auto replay save is enabled
    if ( *CC_AUTO_REPLAY_SAVE_ADDR && AsmHacks::autoReplaySaveStatePtr )
    {
        // Prevent mashing through the auto replay save and causing a hang
        if ( *AsmHacks::autoReplaySaveStatePtr == 100 )
            return 0;
    }

    // Allow saving replays; when manual replay save is selected or any replay save menu is open
    if ( AsmHacks::currentMenuIndex == 2 || *CC_MENU_STATE_COUNTER_ADDR > retryMenuStateCounter )
    {
        AsmHacks::menuConfirmState = 2;
        return input;
    }

    if ( config.mode.isSpectateNetplay() )
    {
        // Disable menu confirms
        AsmHacks::menuConfirmState = 0;

        // Get the retry menu index for the current transition index
        MsgPtr msgMenuIndex = getRetryMenuIndex ( getIndex() );

        // Check if we're done auto-saving (or not auto-saving)
        const bool doneAutoSave = ! ( *CC_AUTO_REPLAY_SAVE_ADDR )
                                  || ( AsmHacks::autoReplaySaveStatePtr && *AsmHacks::autoReplaySaveStatePtr > 100 );

        // Navigate the menu when the menu index is ready AND we're done auto-saving
        if ( msgMenuIndex && doneAutoSave )
        {
            targetMenuState = 0;
            targetMenuIndex = msgMenuIndex->getAs<MenuIndex>().menuIndex;
            return 0;
        }
    }
    else if ( config.mode.isNetplay() )
    {
        // Special netplay retry menu behaviour, only select final option after both sides have selected
        if ( remoteRetryMenuIndex != -1 && localRetryMenuIndex != -1 )
        {
            targetMenuState = 0;
            targetMenuIndex = max ( localRetryMenuIndex, remoteRetryMenuIndex );
            targetMenuIndex = min ( targetMenuIndex, ( int8_t ) 1 ); // Just in case...
            setRetryMenuIndex ( getIndex(), targetMenuIndex );
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

MsgPtr NetplayManager::getLocalRetryMenuIndex() const
{
    if ( state == NetplayState::RetryMenu && localRetryMenuIndex != -1 )
        return MsgPtr ( new MenuIndex ( getIndex(), localRetryMenuIndex ) );
    else
        return 0;
}

void NetplayManager::setRemoteRetryMenuIndex ( int8_t menuIndex )
{
    remoteRetryMenuIndex = menuIndex;

    LOG ( "remoteRetryMenuIndex=%d", remoteRetryMenuIndex );
}

MsgPtr NetplayManager::getRetryMenuIndex ( uint32_t index ) const
{
    if ( config.mode.isOffline() )
        return 0;

    LOG ( "[%s] index=%u", indexedFrame, index );

    ASSERT ( index >= startIndex );

    if ( index >= startIndex + retryMenuIndicies.size() )
        return 0;

    if ( retryMenuIndicies[index - startIndex] < 0 )
        return 0;

    return MsgPtr ( new MenuIndex ( index, retryMenuIndicies[index - startIndex] ) );
}

void NetplayManager::setRetryMenuIndex ( uint32_t index, int8_t menuIndex )
{
    if ( config.mode.isOffline() || index == 0 || index < startIndex || menuIndex < 0 )
        return;

    LOG ( "[%s] index=%u; menuIndex=%d", indexedFrame, index, menuIndex );

    ASSERT ( index >= startIndex );

    if ( index >= startIndex + retryMenuIndicies.size() )
        retryMenuIndicies.resize ( index + 1 - startIndex, -1 );

    retryMenuIndicies[index - startIndex] = menuIndex;
}

uint16_t NetplayManager::getMenuNavInput()
{
    if ( targetMenuState == -1 || targetMenuIndex == -1 )
        return 0;

    if ( targetMenuState == 0 )                                             // Determined targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d", targetMenuIndex );

        targetMenuState = 1;
    }
    else if ( targetMenuState == 1 )                                        // Move up or down towards targetMenuIndex
    {
        targetMenuState = 2;

        if ( targetMenuIndex != ( int8_t ) AsmHacks::currentMenuIndex )
            return COMBINE_INPUT ( ( targetMenuIndex < ( int8_t ) AsmHacks::currentMenuIndex ? 8 : 2 ), 0 );
    }
    else if ( targetMenuState >= 2 && targetMenuState <= 4 )                // Wait for currentMenuIndex to update
    {
        ++targetMenuState;
    }
    else if ( targetMenuState == 39 )                                       // Mash final menu selection
    {
        AsmHacks::menuConfirmState = 2;
        RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );
    }
    else if ( targetMenuIndex != ( int8_t ) AsmHacks::currentMenuIndex )    // Keep navigating
    {
        targetMenuState = 1;
    }
    else                                                                    // Reached targetMenuIndex
    {
        LOG ( "targetMenuIndex=%d; currentMenuIndex=%u", targetMenuIndex, AsmHacks::currentMenuIndex );

        targetMenuState = 39;
    }

    return 0;
}

bool NetplayManager::hasUpDownInHistory ( uint8_t player, uint32_t start, uint32_t end ) const
{
    for ( size_t i = start; i < end; ++i )
    {
        if ( i > getFrame() )
            break;

        if ( player == 0 )
        {
            const uint16_t p1dir = 0xF & getRawInput ( 1, getFrame() - i );

            const uint16_t p2dir = 0xF & getRawInput ( 2, getFrame() - i );

            if ( ( p1dir == 2 ) || ( p1dir == 8 ) || ( p2dir == 2 ) || ( p2dir == 8 ) )
                return true;
        }
        else
        {
            ASSERT ( player == 1 || player == 2 );

            const uint16_t dir = 0xF & getRawInput ( player, getFrame() - i );

            if ( ( dir == 2 ) || ( dir == 8 ) )
                return true;
        }
    }

    return false;
}

bool NetplayManager::hasButtonInHistory ( uint8_t player, uint16_t button, uint32_t start, uint32_t end ) const
{
    ASSERT ( player == 1 || player == 2 );

    for ( size_t i = start; i < end; ++i )
    {
        if ( i > getFrame() )
            break;

        const uint16_t buttons = getRawInput ( player, getFrame() - i ) >> 4;

        if ( buttons & button )
            return true;
    }

    return false;
}

bool NetplayManager::heldButtonInHistory ( uint8_t player, uint16_t button, uint32_t start, uint32_t end ) const
{
    ASSERT ( player == 1 || player == 2 );

    for ( size_t i = start; i < end; ++i )
    {
        if ( i > getFrame() )
            return false;

        const uint16_t buttons = getRawInput ( player, getFrame() - i ) >> 4;

        if ( ! ( buttons & button ) )
            return false;
    }

    return true;
}

void NetplayManager::setRemotePlayer ( uint8_t player )
{
    ASSERT ( player == 1 || player == 2 );

    localPlayer = 3 - player;
    remotePlayer = player;
}

void NetplayManager::updateFrame()
{
    indexedFrame.parts.frame = ( *CC_WORLD_TIMER_ADDR ) - startWorldTime;
}

uint32_t NetplayManager::getBufferedPreserveStartIndex() const
{
    if ( preserveStartIndex == UINT_MAX )
        return UINT_MAX;

    if ( preserveStartIndex <= PRESERVE_START_INDEX_BUFFER )
        return 0;

    return ( preserveStartIndex - PRESERVE_START_INDEX_BUFFER );
}

void NetplayManager::setState ( NetplayState state )
{
    if ( !isValidNext ( state ) )
    {
        LOG ( "Invalid transition: %s -> %s", this->state, state );
        return;
    }

    LOG ( "indexedFrame=[%s]; previous=%s; current=%s", indexedFrame, this->state, state );

    if ( state.value >= NetplayState::CharaSelect )
    {
        if ( this->state == NetplayState::AutoCharaSelect )
        {
            // Start from the initial index and frame
            startWorldTime = 0;
            *CC_WORLD_TIMER_ADDR = initial.indexedFrame.parts.frame;
            indexedFrame = initial.indexedFrame;
        }
        else
        {
            // Increment the index whenever the NetplayState changes
            ++indexedFrame.parts.index;

            // Start counting from frame=0 again
            startWorldTime = *CC_WORLD_TIMER_ADDR;
            indexedFrame.parts.frame = 0;
        }

        // Entering CharaSelect
        if ( state == NetplayState::CharaSelect )
            spectateStartIndex = getIndex();

        // Entering Loading
        if ( state == NetplayState::Loading )
        {
            spectateStartIndex = getIndex();

            const uint32_t newStartIndex = min ( getBufferedPreserveStartIndex(), getIndex() );

            if ( newStartIndex > startIndex )
            {
                const size_t offset = newStartIndex - startIndex;

                inputs[0].eraseIndexOlderThan ( offset );
                inputs[1].eraseIndexOlderThan ( offset );

                if ( offset >= rngStates.size() )
                    rngStates.clear();
                else
                    rngStates.erase ( rngStates.begin(), rngStates.begin() + offset );

                if ( offset >= retryMenuIndicies.size() )
                    retryMenuIndicies.clear();
                else
                    retryMenuIndicies.erase ( retryMenuIndicies.begin(), retryMenuIndicies.begin() + offset );

                startIndex = newStartIndex;
            }

            localRetryMenuIndex = -1;
            remoteRetryMenuIndex = -1;
        }

        // Entering RetryMenu
        if ( state == NetplayState::RetryMenu )
        {
            // The actual retry menu is opened at position *CC_MENU_STATE_COUNTER_ADDR + 1
            retryMenuStateCounter = *CC_MENU_STATE_COUNTER_ADDR + 1;
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

uint16_t NetplayManager::getInput ( uint8_t player )
{
    ASSERT ( player == 1 || player == 2 );

    switch ( state.value )
    {
        case NetplayState::PreInitial:
            return getPreInitialInput ( player );

        case NetplayState::Initial:
            return getInitialInput ( player );

        case NetplayState::AutoCharaSelect:
            return getAutoCharaSelectInput ( player );

        case NetplayState::CharaSelect:
            return getCharaSelectInput ( player );

        case NetplayState::Loading:
        case NetplayState::Skippable:
            // If the remote index is ahead, then we should mash to skip.
            if ( ( startIndex + inputs[remotePlayer - 1].getEndIndex() ) > getIndex() + 1 )
            {
                AsmHacks::menuConfirmState = 2;
                RETURN_MASH_INPUT ( 0, CC_BUTTON_CONFIRM );
            }
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

uint16_t NetplayManager::getRawInput ( uint8_t player, uint32_t frame ) const
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    return inputs[player - 1].get ( getIndex() - startIndex, frame );
}

void NetplayManager::setInput ( uint8_t player, uint16_t input )
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );

    if ( isInRollback() )
    {
        inputs[player - 1].set ( getIndex() - startIndex, getFrame() + config.rollbackDelay, input );
    }
    else if ( state == NetplayState::RetryMenu )
    {
        inputs[player - 1].set ( getIndex() - startIndex, getFrame(), input );
    }
    else
    {
        inputs[player - 1].set ( getIndex() - startIndex, getFrame() + config.delay, input );
    }
}

void NetplayManager::assignInput ( uint8_t player, uint16_t input, uint32_t frame )
{
    assignInput ( player, input, { frame, getIndex() } );
}

void NetplayManager::assignInput ( uint8_t player, uint16_t input, IndexedFrame indexedFrame )
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( indexedFrame.parts.index >= startIndex );

    inputs[player - 1].assign ( indexedFrame.parts.index - startIndex, indexedFrame.parts.frame, input );
}

MsgPtr NetplayManager::getInputs ( uint8_t player ) const
{
    ASSERT ( player == 1 || player == 2 );
    ASSERT ( getIndex() >= startIndex );
    ASSERT ( inputs[player - 1].getEndFrame ( getIndex() - startIndex ) >= 1 );

    PlayerInputs *playerInputs = new PlayerInputs ( { inputs[player - 1].getEndFrame() - 1, getIndex() } );

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
    ASSERT ( getIndex() >= startIndex );
    ASSERT ( playerInputs.getIndex() >= startIndex );

    const uint32_t checkStartingFromIndex = ( isInRollback() ? getIndex() - startIndex : UINT_MAX );

    inputs[player - 1].set ( playerInputs.getIndex() - startIndex, playerInputs.getStartFrame(),
                             &playerInputs.inputs[0], playerInputs.size(), checkStartingFromIndex );
}

MsgPtr NetplayManager::getBothInputs ( IndexedFrame& pos ) const
{
    if ( pos.parts.index > getIndex() )
        return 0;

    IndexedFrame orig = pos;

    ASSERT ( orig.parts.index >= startIndex );

    // This is most recent frame, in the spectator's transition index, that the spectator is allowed to "see"
    uint32_t commonEndFrame = min ( inputs[0].getEndFrame ( orig.parts.index - startIndex ),
                                    inputs[1].getEndFrame ( orig.parts.index - startIndex ) );

    if ( orig.parts.index == getIndex() )                   // During the same transition index
    {
        if ( isInRollback() )
        {
            // Add a buffer to the end frame during rollback
            if ( commonEndFrame > 2 * NUM_INPUTS )
                commonEndFrame -= 2 * NUM_INPUTS;
            else
                commonEndFrame = 0;
        }

        if ( orig.parts.frame + 1 <= commonEndFrame )
        {
            // Increment by NUM_INPUTS when behind
            pos.parts.frame += NUM_INPUTS;
        }
        else
        {
            // Otherwise return empty, so the spectator has to wait
            return 0;
        }
    }
    else                                                    // During an older transition index
    {
        if ( orig.parts.frame + 1 <= commonEndFrame )
        {
            // Increment by NUM_INPUTS when behind
            pos.parts.frame += NUM_INPUTS;
        }
        else
        {
            // Since we're at the end of this transition index, increment to the next one
            pos.parts.frame = NUM_INPUTS - 1;
            ++pos.parts.index;

            // Return empty if this transition index has no inputs
            if ( commonEndFrame == 0 )
                return 0;

            // Otherwise get the rest of this transition index
            orig.parts.frame = commonEndFrame - 1;
        }
    }

    BothInputs *bothInputs = new BothInputs ( orig );

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
    if ( state.value < NetplayState::CharaSelect || state.value == NetplayState::Skippable
            || state.value == NetplayState::Loading || state.value == NetplayState::RetryMenu )
    {
        return true;
    }

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

    const uint8_t maxFramesAhead = ( isInRollback() ? config.rollback : 0 );

    if ( ( inputs[remotePlayer - 1].getEndFrame() - 1 + maxFramesAhead ) < getFrame() )
    {
        LOG ( "[%s] remoteFrame = %u < localFrame=%u; delay=%u; rollback=%u; rollbackDelay=%u",
              indexedFrame, inputs[remotePlayer - 1].getEndFrame() - 1, getFrame(),
              config.delay, config.rollback, config.rollbackDelay );

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

    if ( index >= startIndex + rngStates.size() )
        return 0;

    return rngStates[index - startIndex];
}

void NetplayManager::setRngState ( const RngState& rngState )
{
    if ( config.mode.isOffline() || rngState.index == 0 || rngState.index < startIndex )
        return;

    LOG ( "[%s] rngState.index=%u", indexedFrame, rngState.index );

    ASSERT ( rngState.index >= startIndex );

    if ( rngState.index >= startIndex + rngStates.size() )
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

uint32_t NetplayManager::getRemoteIndex() const
{
    uint32_t remoteIndex = inputs[remotePlayer - 1].getEndIndex() + startIndex;

    if ( remoteIndex > 0 )
        --remoteIndex;

    return remoteIndex;
}

uint32_t NetplayManager::getRemoteFrame() const
{
    uint32_t remoteFrame = inputs[remotePlayer - 1].getEndFrame();

    if ( remoteFrame > 0 )
        --remoteFrame;

    return remoteFrame;
}

IndexedFrame NetplayManager::getRemoteIndexedFrame() const
{
    IndexedFrame indexedFrame =
    {
        inputs[remotePlayer - 1].getEndFrame(),
        inputs[remotePlayer - 1].getEndIndex() + startIndex
    };

    if ( indexedFrame.parts.frame > 0 )
        --indexedFrame.parts.frame;

    if ( indexedFrame.parts.index > 0 )
        --indexedFrame.parts.index;

    return indexedFrame;
}

IndexedFrame NetplayManager::getLastChangedFrame() const
{
    IndexedFrame indexedFrame = inputs[remotePlayer - 1].getLastChangedFrame();

    if ( indexedFrame.value == MaxIndexedFrame.value )
        return MaxIndexedFrame;

    return { indexedFrame.parts.frame, startIndex + indexedFrame.parts.index };
}

void NetplayManager::clearLastChangedFrame()
{
    inputs[remotePlayer - 1].clearLastChangedFrame();
}

void NetplayManager::setRemoteIndex ( uint32_t remoteIndex )
{
    if ( remoteIndex < startIndex )
        return;

    LOG ( "remoteIndex=%u", remoteIndex );

    inputs[remotePlayer - 1].resize ( remoteIndex - startIndex, 0, 0 );
}

bool NetplayManager::isValidNext ( NetplayState next )
{
    static unordered_map<uint8_t, unordered_set<uint8_t>> validTransitions =
    {
        { NetplayState::Unknown, { NetplayState::PreInitial } },
        { NetplayState::PreInitial, { NetplayState::Initial } },
        { NetplayState::Initial, { NetplayState::AutoCharaSelect, NetplayState::CharaSelect } },
        { NetplayState::AutoCharaSelect, { NetplayState::Loading } },
        { NetplayState::CharaSelect, { NetplayState::Loading } },
        { NetplayState::Loading, { NetplayState::Skippable, NetplayState::InGame } },
        { NetplayState::Skippable, { NetplayState::InGame, NetplayState::RetryMenu } },
        { NetplayState::InGame, { NetplayState::Skippable, NetplayState::CharaSelect } },
        { NetplayState::RetryMenu, { NetplayState::Loading, NetplayState::CharaSelect } },
    };

    const auto it = validTransitions.find ( state.value );

    if ( it == validTransitions.end() )
        return false;

    return ( it->second.find ( next.value ) != it->second.end() );
}
