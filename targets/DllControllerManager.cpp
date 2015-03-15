#include "DllControllerManager.h"
#include "DllOverlayUi.h"
#include "DllHacks.h"
#include "AsmHacks.h"
#include "KeyboardState.h"
#include "CharacterSelect.h"

#include <windows.h>

using namespace std;


#define VK_TOGGLE_OVERLAY ( VK_F4 )


void DllControllerManager::initControllers ( const ControllerMappings& mappings )
{
    Lock lock ( ControllerManager::get().mutex );

    ControllerManager::get().owner = this;
    ControllerManager::get().getKeyboard()->setMappings ( ProcessManager::fetchKeyboardConfig() );
    ControllerManager::get().setMappings ( mappings );
    ControllerManager::get().check();

    allControllers = ControllerManager::get().getControllers();
}

bool DllControllerManager::isNotMapping() const
{
    Lock lock ( ControllerManager::get().mutex );

    return  ( !playerControllers[0] || !playerControllers[0]->isMapping() )
            && ( !playerControllers[1] || !playerControllers[1]->isMapping() );
}

void DllControllerManager::updateControls ( uint16_t *localInputs )
{
    Lock lock ( ControllerManager::get().mutex );

    bool toggleOverlay = false;

    if ( KeyboardState::isPressed ( VK_TOGGLE_OVERLAY ) && !ProcessManager::isWine() )
        toggleOverlay = true;

    for ( Controller *controller : allControllers )
    {
        // Don't sticky controllers if the overlay is enabled
        if ( DllOverlayUi::isEnabled() )
            continue;

        // Sticky controllers to the first available player when anything is pressed
        if ( getInput ( controller ) )
        {
            if ( isSinglePlayer )
            {
                // Only sticky the local player in single player modes
                if ( !playerControllers[localPlayer - 1] && controller != playerControllers[localPlayer - 1] )
                {
                    playerControllers[localPlayer - 1] = controller;
                }
            }
            else
            {
                // Sticky player 1 then player 2 optimistically
                if ( !playerControllers[0] && controller != playerControllers[1] )
                {
                    playerControllers[0] = controller;
                }
                else if ( !playerControllers[1] && controller != playerControllers[0] )
                {
                    playerControllers[1] = controller;
                }
            }
        }
    }

    // Show message takes priority over the overlay UI
    if ( DllOverlayUi::isShowingMessage() )
    {
        DllOverlayUi::updateMessage();
    }
    else
    {
        // Only toggle overlay if both players are "done"; ie at the first option; or have no controller
        if ( toggleOverlay
                && ( !playerControllers[0] || overlayPositions[0] == 0 )
                && ( !playerControllers[1] || overlayPositions[1] == 0 ) )
        {
            if ( ! DllOverlayUi::isEnabled() || DllOverlayUi::isShowingPaletteEditor() )
            {
                // Refresh the list of joysticks if we're enabling the overlay
                ControllerManager::get().refreshJoysticks();

                // Enable keyboard events, this effectively eats all keyboard inputs for the window
                KeyboardManager::get().keyboardWindow = DllHacks::windowHandle;
                KeyboardManager::get().matchedKeys.clear();
                KeyboardManager::get().ignoredKeys.clear();
                KeyboardManager::get().hook ( this, true );

                // Disable Escape to exit
                AsmHacks::enableEscapeToExit = false;

                // Hide palette editor and enable overlay
                DllOverlayUi::hidePaletteEditor();
                DllOverlayUi::enable();
            }
            else
            {
                // Cancel all mapping if we're disabling the overlay
                if ( playerControllers[0] )
                    playerControllers[0]->cancelMapping();
                if ( playerControllers[1] )
                    playerControllers[1]->cancelMapping();
                overlayPositions[0] = 0;
                overlayPositions[1] = 0;

                // Disable keyboard events, since we use GetKeyState for regular controller inputs
                KeyboardManager::get().unhook();

                // Re-enable Escape to exit
                AsmHacks::enableEscapeToExit = true;

                // Disable overlay
                DllOverlayUi::disable();
            }

            toggleOverlay = false;
        }
    }

    // Handle the palette editor
    if ( DllOverlayUi::isShowingPaletteEditor() )
    {
        static const uint32_t FlushFrames = 300;

        static uint32_t flushing = 0;
        static uint32_t origPalette = 0;

        uint32_t& paletteNumber = *CC_P1_COLOR_SELECTOR_ADDR;

        // TODO figure out if there's a better way to do this
        if ( flushing )
        {
            *CC_SKIP_FRAMES_ADDR = 1;

            if ( flushing == FlushFrames )
                origPalette = paletteNumber;

            if ( flushing % 100 == 0 )
                paletteNumber = ( paletteNumber + 1 ) % 36;

            --flushing;

            if ( flushing == 0 )
                paletteNumber = origPalette;
            return;
        }

        if ( !DllPaletteManager::isReady() )
        {
            DllOverlayUi::updateText ( { "", "Loading...", "" } );
            return;
        }

        if ( KeyboardState::isPressedOrHeld ( VK_NEXT ) )
        {
            uint32_t selector = *CC_P1_CHARA_SELECTOR_ADDR;
            uint32_t chara = UNKNOWN_POSITION;

            while ( chara == UNKNOWN_POSITION )
            {
                selector = ( selector + 1 ) % 43;
                chara = selectorToChara ( selector );
            }

            *CC_P1_CHARA_SELECTOR_ADDR = selector;
            * CC_P1_CHARACTER_ADDR = chara;
        }
        else if ( KeyboardState::isPressedOrHeld ( VK_PRIOR ) )
        {
            uint32_t selector = *CC_P1_CHARA_SELECTOR_ADDR;
            uint32_t chara = UNKNOWN_POSITION;

            while ( chara == UNKNOWN_POSITION )
            {
                selector = ( selector + 43 - 1 ) % 43;
                chara = selectorToChara ( selector );
            }

            *CC_P1_CHARA_SELECTOR_ADDR = selector;
            * CC_P1_CHARACTER_ADDR = chara;
        }

        DllPaletteManager& palMan = palMans [ *CC_P1_CHARACTER_ADDR ];

        if ( KeyboardState::isHeld ( VK_DELETE, 1000, 1 ) )
        {
            DllOverlayUi::clearCurrentColor();
            palMan.clear ( paletteNumber );
            for ( size_t i = 0; i < 256; ++i )
                palMan.set ( paletteNumber, i, palMan.get ( paletteNumber, i ) );
            flushing = FlushFrames;
            DllOverlayUi::updateText ( { "", format ( ">> Color %02d reset to defaults <<", paletteNumber + 1 ), "" } );
            return;
        }

        const uint32_t color = ( DllOverlayUi::hasCurrentColor()
                                 ? DllOverlayUi::getCurrentColor()
                                 : palMan.get ( paletteNumber, colorNumber ) );

        const string text = format ( "Color %02d - %03d : %06X", paletteNumber + 1, colorNumber + 1, color );

        DllOverlayUi::updateText ( { "", text, "" } );
        DllOverlayUi::setCurrentColor ( color );

        bool changedColor = false;

        if ( KeyboardState::isPressedOrHeld ( VK_RIGHT ) )
        {
            changedColor = true;
            colorNumber = ( colorNumber + 1 ) % 256;
        }
        else if ( KeyboardState::isPressedOrHeld ( VK_LEFT ) )
        {
            changedColor = true;
            colorNumber = ( colorNumber + 256 - 1 ) % 256;
        }

        if ( KeyboardState::isPressedOrHeld ( VK_DOWN ) )
        {
            changedColor = true;
            paletteNumber = ( paletteNumber + 1 ) % 36;
            colorNumber = 0;
        }
        else if ( KeyboardState::isPressedOrHeld ( VK_UP ) )
        {
            changedColor = true;
            paletteNumber = ( paletteNumber + 36 - 1 ) % 36;
            colorNumber = 0;
        }

        if ( changedColor )
        {
            DllOverlayUi::clearCurrentColor();
        }
        else if ( KeyboardState::isReleased ( VK_DELETE ) )
        {
            DllOverlayUi::clearCurrentColor();
            palMan.clear ( paletteNumber, colorNumber );
            palMan.set ( paletteNumber, colorNumber, palMan.get ( paletteNumber, colorNumber ) );
            flushing = FlushFrames;
            DllOverlayUi::updateText ( { "", "Reloading...", "" } );
        }
        else if ( KeyboardState::isPressed ( VK_RETURN ) )
        {
            palMan.set ( paletteNumber, colorNumber, color );
            flushing = FlushFrames;
            DllOverlayUi::updateText ( { "", "Reloading...", "" } );
        }
        return;
    }

    // Only update player controls when the overlay is NOT enabled
    if ( ! DllOverlayUi::isEnabled() )
    {
        if ( playerControllers[localPlayer - 1] )
            localInputs[0] = getInput ( playerControllers[localPlayer - 1] );

        if ( playerControllers[remotePlayer - 1] )
            localInputs[1] = getInput ( playerControllers[remotePlayer - 1] );

        return;
    }

    // Check all controllers
    for ( Controller *controller : allControllers )
    {
        if ( ( controller->isJoystick() && isDirectionPressed ( controller, 6 ) )
                || ( controller->isKeyboard() && KeyboardState::isPressed ( VK_RIGHT ) ) )
        {
            // Move controller right
            if ( controller == playerControllers[0] )
            {
                if ( ! ( controller->isKeyboard() && controller->isMapping()
                         && overlayPositions[0] >= 1 && overlayPositions[0] <= 4 ) )
                {
                    playerControllers[0]->cancelMapping();
                    playerControllers[0] = 0;
                }
            }
            else if ( isSinglePlayer && localPlayer == 1 )
            {
                // Only one controller (player 1)
                continue;
            }
            else if ( !playerControllers[1] )
            {
                playerControllers[1] = controller;
                overlayPositions[1] = 0;
            }
        }
        else if ( ( controller->isJoystick() && isDirectionPressed ( controller, 4 ) )
                  || ( controller->isKeyboard() && KeyboardState::isPressed ( VK_LEFT ) ) )
        {
            // Move controller left
            if ( controller == playerControllers[1] )
            {
                if ( ! ( controller->isKeyboard() && controller->isMapping()
                         && overlayPositions[1] >= 1 && overlayPositions[1] <= 4 ) )
                {
                    playerControllers[1]->cancelMapping();
                    playerControllers[1] = 0;
                }
            }
            else if ( isSinglePlayer && localPlayer == 2 )
            {
                // Only one controller (player 2)
                continue;
            }
            else if ( !playerControllers[0] )
            {
                playerControllers[0] = controller;
                overlayPositions[0] = 0;
            }
        }
    }

    array<string, 3> text;

    // Display all controllers
    text[1] = "Controllers\n";
    for ( const Controller *controller : allControllers )
        if ( controller != playerControllers[0] && controller != playerControllers[1] )
            text[1] += "\n" + controller->getName();

    const size_t controllersHeight = 3 + allControllers.size();

    // Update player controllers
    for ( uint8_t i = 0; i < 2; ++i )
    {
        string& playerText = text [ i ? 2 : 0 ];

        // Hide / disable other player's overlay in netplay
        if ( isSinglePlayer && localPlayer != i + 1 )
        {
            playerText.clear();
            DllOverlayUi::updateSelector ( i );
            continue;
        }

        // Show placeholder when player has no controller assigned
        if ( !playerControllers[i] )
        {
            playerText = ( i == 0 ? "Press Left on P1 controller" : "Press Right on P2 controller" );
            DllOverlayUi::updateSelector ( i );
            continue;
        }

        // Generate mapping options starting with controller name
        size_t headerHeight = 0;
        vector<string> options;
        options.push_back ( playerControllers[i]->getName() );

        if ( playerControllers[i]->isKeyboard() )
        {
            headerHeight = max ( 3u, controllersHeight );

            // Instructions for mapping keyboard controls
            playerText = "Press Enter to set a direction key\n";
            playerText += format ( "Press %s to delete a key\n", ( i == 0 ? "Left" : "Right" ) );
            playerText += string ( headerHeight - 3, '\n' );

            // Add directions to keyboard mapping options
            for ( size_t j = 0; j < 4; ++j )
            {
                const string mapping = playerControllers[i]->getMapping ( gameInputBits[j].second, "..." );
                options.push_back ( gameInputBits[j].first + " : " + mapping );
            }
        }
        else
        {
            headerHeight = max ( 2u, controllersHeight );

            // Instructions for mapping joystick buttons
            playerText = format ( "Press %s to delete a key\n", ( i == 0 ? "Left" : "Right" ) );
            playerText += string ( headerHeight - 2, '\n' );
        }

        // Add buttons to mapping options
        for ( size_t j = 4; j < gameInputBits.size(); ++j )
        {
            const string mapping = playerControllers[i]->getMapping ( gameInputBits[j].second );
            options.push_back ( gameInputBits[j].first + " : " + mapping );
        }

        // Finally add done option
        options.push_back ( playerControllers[i]->isKeyboard() ? "Done (press Enter)" : "Done (press any button)" );

        // Disable overlay if both players are done
        if ( overlayPositions[i] + 1 == options.size()
                && ( ( playerControllers[i]->isJoystick() && isAnyButtonPressed ( playerControllers[i] ) )
                     || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_RETURN ) ) ) )
        {
            overlayPositions[i] = 0;

            if ( ( !playerControllers[0] || !overlayPositions[0] )
                    && ( !playerControllers[1] || !overlayPositions[1] ) )
            {
                overlayPositions[0] = 0;
                overlayPositions[1] = 0;
                DllOverlayUi::disable();
                KeyboardManager::get().unhook();
                AsmHacks::enableEscapeToExit = true;
                return;
            }
        }

        // Update overlay text with all the options
        for ( const string& option : options )
            playerText += "\n" + option;

        // Filter keyboard overlay controls when mapping directions
        if ( playerControllers[i]->isKeyboard() && playerControllers[i]->isMapping()
                && overlayPositions[i] >= 1 && overlayPositions[i] <= 4 )
        {
            DllOverlayUi::updateSelector ( i, headerHeight + overlayPositions[i], options[overlayPositions[i]] );
            continue;
        }

        bool deleteMapping = false, mapDirections = false, changedPosition = false;

        if ( ( i == 0
                && ( ( playerControllers[i]->isJoystick() && isDirectionPressed ( playerControllers[i], 4 ) )
                     || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_LEFT ) ) ) )
                || ( i == 1
                     && ( ( playerControllers[i]->isJoystick() && isDirectionPressed ( playerControllers[i], 6 ) )
                          || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_RIGHT ) ) ) ) )
        {
            // Delete selected mapping
            deleteMapping = true;
        }
        else if ( playerControllers[i]->isKeyboard()
                  && ( KeyboardState::isReleased ( VK_RETURN )      // Use Return key released to prevent the
                       || KeyboardState::isPressed ( VK_DELETE ) )  // same key event from being mapped immediately.
                  && overlayPositions[i] >= 1 && overlayPositions[i] <= 4 )
        {
            // Press enter / delete to modify direction keys
            if ( KeyboardState::isReleased ( VK_RETURN ) )
                mapDirections = true;
            else
                deleteMapping = true;
        }
        else if ( ( playerControllers[i]->isJoystick() && isDirectionPressed ( playerControllers[i], 2 ) )
                  || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressedOrHeld ( VK_DOWN ) ) )
        {
            // Move selector down
            overlayPositions[i] = ( overlayPositions[i] + 1 ) % options.size();
            changedPosition = true;
        }
        else if ( ( playerControllers[i]->isJoystick() && isDirectionPressed ( playerControllers[i], 8 ) )
                  || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressedOrHeld ( VK_UP ) ) )
        {
            // Move selector up
            overlayPositions[i] = ( overlayPositions[i] + options.size() - 1 ) % options.size();
            changedPosition = true;
        }

        if ( deleteMapping || mapDirections || changedPosition || finishedMapping[i] )
        {
            if ( overlayPositions[i] >= 1 && overlayPositions[i] < gameInputBits.size() + 1 )
            {
                // Convert selector position to game input bit position
                const size_t pos = overlayPositions[i] - 1 + ( playerControllers[i]->isKeyboard() ? 0 : 4 );

                if ( deleteMapping && pos < gameInputBits.size() )
                {
                    // Delete mapping
                    playerControllers[i]->clearMapping ( gameInputBits[pos].second );
                    saveMappings ( playerControllers[i] );
                }
                else if ( pos >= 4 && pos < gameInputBits.size() )
                {
                    // Map a button only
                    playerControllers[i]->startMapping ( this, gameInputBits[pos].second,
                                                         MAP_CONTINUOUSLY | MAP_PRESERVE_DIRS );
                }
                else if ( ( mapDirections || finishedMapping[i] ) && pos < 4 )
                {
                    ASSERT ( playerControllers[i]->isKeyboard() == true );

                    // Map a keyboard direction
                    playerControllers[i]->startMapping ( this, gameInputBits[pos].second );
                }
                else
                {
                    // In all other situations cancel the current mapping
                    playerControllers[i]->cancelMapping();
                }
            }
            else
            {
                // In all other situations cancel the current mapping
                playerControllers[i]->cancelMapping();
            }

            finishedMapping[i] = false;
        }

        if ( overlayPositions[i] == 0 )
        {
            playerText = string ( "Press Up or Down to set keys" )
                         + string ( controllersHeight, '\n' )
                         + playerControllers[i]->getName();
            DllOverlayUi::updateSelector ( i, controllersHeight, playerControllers[i]->getName() );
        }
        else
        {
            DllOverlayUi::updateSelector ( i, headerHeight + overlayPositions[i], options[overlayPositions[i]] );
        }
    }

    DllOverlayUi::updateText ( text );

    // Enable Escape to exit if neither controller is being mapped
    AsmHacks::enableEscapeToExit =
        ( !playerControllers[0] || overlayPositions[0] == 0 )
        && ( !playerControllers[1] || overlayPositions[1] == 0 );

    ControllerManager::get().savePrevStates();
}

void DllControllerManager::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
    Lock lock ( ControllerManager::get().mutex );

    for ( uint8_t i = 0; i < 2; ++i )
    {
        if ( playerControllers[i] && playerControllers[i]->isKeyboard()
                && overlayPositions[i] >= 1 && overlayPositions[i] < gameInputBits.size() + 1 )
        {
            // Convert selector position to game input bit position
            const size_t pos = overlayPositions[i] - 1;

            // Ignore specific control keys when mapping keyboard buttons
            if ( pos >= 4 && pos < gameInputBits.size()
                    && ( vkCode == VK_TOGGLE_OVERLAY
                         || vkCode == VK_ESCAPE
                         || vkCode == VK_UP
                         || vkCode == VK_DOWN
                         || vkCode == VK_LEFT
                         || vkCode == VK_RIGHT ) )
            {
                return;
            }

            playerControllers[i]->keyboardEvent ( vkCode, scanCode, isExtended, isDown );
            return;
        }
    }
}

void DllControllerManager::mouseEvent ( int x, int y, bool isDown, bool pressed, bool released )
{
    // This doesn't modify any of the controllers, so we don't need to lock the main mutex

    DllOverlayUi::paletteMouseEvent ( x, y, isDown, pressed, released );
}

void DllControllerManager::attachedJoystick ( Controller *controller )
{
    // This is a callback from within ControllerManager, so we don't need to lock the main mutex

    allControllers.push_back ( controller );
}

void DllControllerManager::detachedJoystick ( Controller *controller )
{
    // This is a callback from within ControllerManager, so we don't need to lock the main mutex

    if ( playerControllers[0] == controller )
        playerControllers[0] = 0;

    if ( playerControllers[1] == controller )
        playerControllers[1] = 0;

    const auto it = find ( allControllers.begin(), allControllers.end(), controller );

    ASSERT ( it != allControllers.end() );

    allControllers.erase ( it );
}

void DllControllerManager::doneMapping ( Controller *controller, uint32_t key )
{
    // This is a callback from within ControllerManager, so we don't need to lock the main mutex

    LOG ( "%s: controller=%08x; key=%08x", controller->getName(), controller, key );

    if ( key )
    {
        saveMappings ( controller );

        for ( uint8_t i = 0; i < 2; ++i )
        {
            if ( controller == playerControllers[i]
                    && overlayPositions[i] >= 1 && overlayPositions[i] < gameInputBits.size() + 1  )
            {
                // Convert selector position to game input bit position
                const size_t pos = overlayPositions[i] - 1 + ( playerControllers[i]->isKeyboard() ? 0 : 4 );

                // Continue mapping
                if ( pos + 1 < gameInputBits.size() )
                    ++overlayPositions[i];

                finishedMapping[i] = true;
                return;
            }
        }
    }
}
