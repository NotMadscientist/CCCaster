#include "DllControllerManager.h"
#include "DllOverlayUi.h"
#include "DllHacks.h"
#include "AsmHacks.h"
#include "KeyboardState.h"

#include <windows.h>

using namespace std;


#define VK_TOGGLE_OVERLAY ( VK_F4 )


void DllControllerManager::checkOverlay ( bool allowStartButton )
{
    bool toggleOverlay = false;

    if ( KeyboardState::isPressed ( VK_TOGGLE_OVERLAY ) )
        toggleOverlay = true;

    for ( Controller *controller : allControllers )
    {
        if ( allowStartButton && controller->isJoystick() && isButtonPressed ( controller, CC_BUTTON_START ) )
        {
            toggleOverlay = true;
        }

        // Don't sticky controllers if the overlay is enabled
        if ( DllOverlayUi::isEnabled() )
            continue;

        uint16_t input = getInput ( controller );

        // Sticky controllers to the first available player when anything is pressed EXCEPT start
        if ( input && ! ( input & COMBINE_INPUT ( 0, CC_BUTTON_START ) ) )
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

    // Show message takes priority
    if ( DllOverlayUi::isShowingMessage() )
    {
        DllOverlayUi::updateMessage();
        return;
    }

    // Only toggle overlay if both players are "done"; ie at the first option; or have no controller
    if ( toggleOverlay
            && ( !playerControllers[0] || overlayPositions[0] == 0 )
            && ( !playerControllers[1] || overlayPositions[1] == 0 ) )
    {
        if ( DllOverlayUi::isEnabled() )
        {
            // Cancel all mapping if we're disabling the overlay
            if ( playerControllers[0] )
                playerControllers[0]->cancelMapping();
            if ( playerControllers[1] )
                playerControllers[1]->cancelMapping();

            // Disable keyboard events, since we use GetKeyState for regular controller inputs
            KeyboardManager::get().unhook();

            // Re-enable Escape to exit
            AsmHacks::enableEscapeToExit = true;
        }
        else
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
        }

        DllOverlayUi::toggle();
        toggleOverlay = false;
    }

    if ( !DllOverlayUi::isEnabled() )
        return;

    // Check all controllers
    for ( Controller *controller : allControllers )
    {
        if ( ( controller->isJoystick() && isDirectionPressed ( controller, 6 ) )
                || ( controller->isKeyboard() && KeyboardState::isPressed ( VK_RIGHT ) ) )
        {
            // Move controller right
            if ( controller == playerControllers[0] )
            {
                playerControllers[0]->cancelMapping();
                playerControllers[0] = 0;
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
                playerControllers[1]->cancelMapping();
                playerControllers[1] = 0;
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
    text[2] = "Controllers\n";
    for ( const Controller *controller : allControllers )
        if ( controller != playerControllers[0] && controller != playerControllers[1] )
            text[2] += "\n" + controller->getName();

    const size_t controllersHeight = 3 + allControllers.size();

    // Update player controllers
    for ( uint8_t i = 0; i < 2; ++i )
    {
        // Hide / disable other player's overlay in netplay
        if ( isSinglePlayer && localPlayer != i + 1 )
        {
            text[i].clear();
            DllOverlayUi::updateSelector ( i );
            continue;
        }

        // Show placeholder when player has no controller assigned
        if ( !playerControllers[i] )
        {
            text[i] = ( i == 0 ? "Press Left on P1 controller" : "Press Right on P2 controller" );
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
            text[i] = "Press Enter to set a direction key\n";
            text[i] += format ( "Press %s to delete a key\n", ( i == 0 ? "Left" : "Right" ) );
            text[i] += string ( headerHeight - 3, '\n' );

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
            text[i] = format ( "Press %s to delete a key\n", ( i == 0 ? "Left" : "Right" ) );
            text[i] += string ( headerHeight - 2, '\n' );
        }

        // Add buttons to mapping options
        for ( size_t j = 4; j < gameInputBits.size(); ++j )
        {
            const string mapping = playerControllers[i]->getMapping ( gameInputBits[j].second );
            options.push_back ( gameInputBits[j].first + " : " + mapping );
        }

        // Finally add done option
        options.push_back ( playerControllers[i]->isKeyboard() ? "Done (press Enter)" : "Done (press any button)" );

        // Toggle overlay if both players are done
        if ( overlayPositions[i] + 1 == options.size()
                && ( ( playerControllers[i]->isJoystick() && isAnyButtonPressed ( playerControllers[i] ) )
                     || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_RETURN ) ) ) )
        {
            overlayPositions[i] = 0;

            if ( ( !playerControllers[0] || !overlayPositions[0] )
                    && ( !playerControllers[1] || !overlayPositions[1] ) )
            {
                text[0] = text[1] = text[2] = "";
                DllOverlayUi::updateText ( text );
                DllOverlayUi::toggle();
                KeyboardManager::get().unhook();
                AsmHacks::enableEscapeToExit = true;
                return;
            }
        }

        // Update overlay text with all the options
        for ( const string& option : options )
            text[i] += "\n" + option;

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
                  || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_DOWN ) ) )
        {
            // Move selector down
            overlayPositions[i] = ( overlayPositions[i] + 1 ) % options.size();
            changedPosition = true;
        }
        else if ( ( playerControllers[i]->isJoystick() && isDirectionPressed ( playerControllers[i], 8 ) )
                  || ( playerControllers[i]->isKeyboard() && KeyboardState::isPressed ( VK_UP ) ) )
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
            text[i] = string ( "Press Up or Down to set keys" )
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
}

void DllControllerManager::keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown )
{
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

void DllControllerManager::attachedJoystick ( Controller *controller )
{
    allControllers.push_back ( controller );
}

void DllControllerManager::detachedJoystick ( Controller *controller )
{
    if ( playerControllers[0] == controller )
        playerControllers[0] = 0;

    if ( playerControllers[1] == controller )
        playerControllers[1] = 0;

    auto it = find ( allControllers.begin(), allControllers.end(), controller );

    ASSERT ( it != allControllers.end() );

    allControllers.erase ( it );
}

void DllControllerManager::doneMapping ( Controller *controller, uint32_t key )
{
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
