#include "DllPaletteControls.h"
#include "DllOverlayUi.h"
#include "DllHacks.h"
#include "AsmHacks.h"
#include "KeyboardState.h"
#include "CharacterSelect.h"
#include "ProcessManager.h"

#include <windows.h>

using namespace std;


#define VK_TOGGLE_PALETTES      ( VK_F3 )

#define FLUSH_FRAMES            ( 300 )

#define FLUSHING_COLORS_MESSAGE "Preparing colors..."


bool DllPaletteControls::isPaletteEditorEnabled() const
{
    if ( ProcessManager::isWine() )
        return false;

    return ( enabling || disabling || DllOverlayUi::isShowingPaletteEditor() );
}

void DllPaletteControls::updatePaletteEditor()
{
    if ( ProcessManager::isWine() )
        return;

    if ( ! DllPaletteManager::isReady() )
        return;

    DllPaletteManager& palMan = palMans [ *CC_P1_CHARACTER_ADDR ];

    if ( flushing )
    {
        DllOverlayUi::updateText();

        *CC_SKIP_FRAMES_ADDR = 1;

        if ( flushing % 100 == 0 )
            *CC_P1_COLOR_SELECTOR_ADDR = ( *CC_P1_COLOR_SELECTOR_ADDR + 1 ) % 36;

        --flushing;

        if ( flushing == 0 )
            palMan.doneFlushing();
        return;
    }

    if ( enabling )
    {
        DllOverlayUi::updateText();

        if ( DllOverlayUi::isToggling() )
            return;

        if ( ! DllOverlayUi::isShowingPaletteEditor() )
        {
            *CC_P1_SELECTOR_MODE_ADDR = 1;

            AsmHacks::disableSlideInAnimation = 1;

            DllOverlayUi::showPaletteEditor();

            MouseManager::get().owner = this;

            DllOverlayUi::updateText ( { "", FLUSHING_COLORS_MESSAGE, "" } );
        }
        else
        {
            enabling = false;

            // This needs a flush to display correctly
            palMan.install();

            // Flush AFTER showing the message for one frame
            flushing = FLUSH_FRAMES;
            *CC_SKIP_FRAMES_ADDR = 1;
        }
        return;
    }

    if ( disabling )
    {
        disabling = false;

        *CC_P1_SELECTOR_MODE_ADDR = 2;

        AsmHacks::disableSlideInAnimation = 0;

        DllOverlayUi::hidePaletteEditor();
        DllOverlayUi::disable();

        MouseManager::get().owner = 0;
        return;
    }

    if ( KeyboardState::isPressed ( VK_F3 ) )
    {
        if ( ! DllOverlayUi::isShowingPaletteEditor() )
        {
            if ( *CC_P1_SELECTOR_MODE_ADDR != 2 )
                return;

            DllOverlayUi::enable();
            DllOverlayUi::updateSelector ( 0, 0, "" );
            DllOverlayUi::updateSelector ( 1, 0, "" );
            DllOverlayUi::updateText ( { "", "", "" } );

            enabling = true;
        }
        else
        {
            // This needs a flush to display correctly
            palMan.uninstall();

            // Flush BEFORE disabling the palette editor
            flushing = FLUSH_FRAMES;
            DllOverlayUi::updateText ( { "", FLUSHING_COLORS_MESSAGE, "" } );

            disabling = true;
        }

        AsmHacks::enableEscapeToExit = true;
        return;
    }

#ifdef DEBUG
    if ( KeyboardState::isPressed ( VK_F2 ) )
    {
        flushing = FLUSH_FRAMES;
        return;
    }
#endif

    if ( ! DllOverlayUi::isShowingPaletteEditor() )
        return;

    // Display the header text
    const uint32_t color = palMan.get ( palMan.getPaletteNumber(), palMan.getColorNumber() );

    const string text = format ( "Color %02d - %03d : %06X",
                                 palMan.getPaletteNumber() + 1, palMan.getColorNumber() + 1, color );

    DllOverlayUi::updateText ( { "", text, "" } );

    // Handle palette editor keyboard controls
    if ( DllOverlayUi::isShowingPaletteEditor() )
    {
        if ( KeyboardState::isPressedOrHeld ( VK_RIGHT ) || KeyboardState::isPressedOrHeld ( 'D' ) )
        {
            palMan.setColorNumber ( palMan.getColorNumber() + 1 );
        }
        else if ( KeyboardState::isPressedOrHeld ( VK_LEFT ) || KeyboardState::isPressedOrHeld ( 'A' ) )
        {
            palMan.setColorNumber ( palMan.getColorNumber() + 256 - 1 );
        }

        palMan.frameStep();
    }






    // static const uint32_t FLUSH_FRAMES = 300;
    // static uint32_t flushing = 0;
    // bool flush = false;

    // DllPaletteManager& palMan = palMans [ *CC_P1_CHARACTER_ADDR ];

    // // TODO figure out if there's a better way to do this
    // if ( flushing )
    // {
    //     DllOverlayUi::updateText ( { "", FLUSHING_COLORS_MESSAGE, "" } );

    //     *CC_SKIP_FRAMES_ADDR = 1;

    //     if ( flushing % 100 == 0 )
    //         *CC_P1_COLOR_SELECTOR_ADDR = ( *CC_P1_COLOR_SELECTOR_ADDR + 1 ) % 36;

    //     --flushing;

    //     if ( flushing == 0 )
    //         palMan.doneFlushing();
    //     return;
    // }

    // if ( KeyboardState::isPressed ( VK_F3 ) )
    // {
    //     if ( !DllOverlayUi::isShowingPaletteEditor() )
    //     {
    //         if ( *CC_P1_SELECTOR_MODE_ADDR != 2 )
    //             return;

    //         DllOverlayUi::showPaletteEditor();
    //         MouseManager::get().owner = this;
    //         palMan.install();
    //     }
    //     else
    //     {
    //         DllOverlayUi::hidePaletteEditor();
    //         MouseManager::get().owner = 0;
    //         palMan.uninstall();
    //     }

    //     AsmHacks::enableEscapeToExit = true;
    //     flush = true;
    // }
    // else if ( KeyboardState::isPressed ( VK_F2 ) )
    // {
    //     flush = true;
    // }

    // // Handle the palette editor
    // if ( DllOverlayUi::isShowingPaletteEditor() )
    // {
    //     if ( KeyboardState::isPressedOrHeld ( VK_RIGHT ) )
    //     {
    //         palMan.setColorNumber ( palMan.getColorNumber() + 1 );
    //     }
    //     else if ( KeyboardState::isPressedOrHeld ( VK_LEFT ) )
    //     {
    //         palMan.setColorNumber ( palMan.getColorNumber() + 256 - 1 );
    //     }

    //     palMan.frameStep();

    //     // bool changedCharacter = firstTime;

    //     // if ( firstTime )
    //     //     firstTime = false;

    //     // if ( KeyboardState::isPressedOrHeld ( VK_NEXT ) )
    //     // {
    //     //     uint32_t selector = *CC_P1_CHARA_SELECTOR_ADDR;
    //     //     uint32_t chara = UNKNOWN_POSITION;

    //     //     while ( chara == UNKNOWN_POSITION )
    //     //     {
    //     //         selector = ( selector + 1 ) % 43;
    //     //         chara = selectorToChara ( selector );
    //     //     }

    //     //     *CC_P1_CHARA_SELECTOR_ADDR = selector;
    //     //     *CC_P1_CHARACTER_ADDR = chara;
    //     //     changedCharacter = true;
    //     // }
    //     // else if ( KeyboardState::isPressedOrHeld ( VK_PRIOR ) )
    //     // {
    //     //     uint32_t selector = *CC_P1_CHARA_SELECTOR_ADDR;
    //     //     uint32_t chara = UNKNOWN_POSITION;

    //     //     while ( chara == UNKNOWN_POSITION )
    //     //     {
    //     //         selector = ( selector + 43 - 1 ) % 43;
    //     //         chara = selectorToChara ( selector );
    //     //     }

    //     //     *CC_P1_CHARA_SELECTOR_ADDR = selector;
    //     //     *CC_P1_CHARACTER_ADDR = chara;
    //     //     changedCharacter = true;
    //     // }

    //     // DllPaletteManager& palMan = palMans [ *CC_P1_CHARACTER_ADDR ];

    //     // if ( changedCharacter )
    //     // {
    //     //     paletteNumber = 0;
    //     //     colorNumber = 0;
    //     //     DllOverlayUi::clearCurrentColor();
    //     //     palMan.install();
    //     //     flushing = FLUSH_FRAMES;
    //     // }

    //     // if ( KeyboardState::isHeld ( VK_DELETE, 1000, 1 ) )
    //     // {
    //     //     DllOverlayUi::clearCurrentColor();
    //     //     palMan.clear ( paletteNumber );
    //     //     for ( size_t i = 0; i < 256; ++i )
    //     //         palMan.set ( paletteNumber, i, palMan.get ( paletteNumber, i ) );
    //     //     flushing = FLUSH_FRAMES;
    //     //     DllOverlayUi::updateText ( { "", format ( ">> Color %02d reset to defaults <<", paletteNumber + 1 ), "" } );
    //     //     return;
    //     // }

    //     // const uint32_t color = ( DllOverlayUi::hasCurrentColor()
    //     //                          ? DllOverlayUi::getCurrentColor()
    //     //                          : palMan.get ( paletteNumber, colorNumber ) );

    //     // const string text = format ( "Color %02d - %03d : %06X", paletteNumber + 1, colorNumber + 1, color );

    //     // DllOverlayUi::updateText ( { "", text, "" } );
    //     // DllOverlayUi::setCurrentColor ( color );

    //     // bool changedColor = false;

    //     // if ( KeyboardState::isPressedOrHeld ( VK_RIGHT ) )
    //     // {
    //     //     colorNumber = ( colorNumber + 1 ) % 256;
    //     //     changedColor = true;
    //     // }
    //     // else if ( KeyboardState::isPressedOrHeld ( VK_LEFT ) )
    //     // {
    //     //     colorNumber = ( colorNumber + 256 - 1 ) % 256;
    //     //     changedColor = true;
    //     // }

    //     // if ( KeyboardState::isPressedOrHeld ( VK_DOWN ) )
    //     // {
    //     //     paletteNumber = ( paletteNumber + 1 ) % 36;
    //     //     colorNumber = 0;
    //     //     changedColor = true;
    //     // }
    //     // else if ( KeyboardState::isPressedOrHeld ( VK_UP ) )
    //     // {
    //     //     paletteNumber = ( paletteNumber + 36 - 1 ) % 36;
    //     //     colorNumber = 0;
    //     //     changedColor = true;
    //     // }

    //     // if ( changedColor )
    //     // {
    //     //     palMan.update ( colorNumber );
    //     //     DllOverlayUi::clearCurrentColor();
    //     //     LOG ( "Color %02d - %03d : %06X", paletteNumber + 1, colorNumber + 1, color );
    //     // }
    //     // else if ( KeyboardState::isReleased ( VK_DELETE ) )
    //     // {
    //     //     DllOverlayUi::clearCurrentColor();
    //     //     palMan.clear ( paletteNumber, colorNumber );
    //     //     palMan.set ( paletteNumber, colorNumber, palMan.get ( paletteNumber, colorNumber ) );
    //     //     flushing = FLUSH_FRAMES;
    //     //     DllOverlayUi::updateText ( { "", "Reloading...", "" } );
    //     // }
    //     // else if ( KeyboardState::isPressed ( VK_RETURN ) )
    //     // {
    //     //     palMan.set ( paletteNumber, colorNumber, color );
    //     //     flushing = FLUSH_FRAMES;
    //     //     DllOverlayUi::updateText ( { "", "Reloading...", "" } );
    //     // }
    // }

    // if ( flush )
    // {
    //     flushing = FLUSH_FRAMES;
    //     DllOverlayUi::updateText ( { "", FLUSHING_COLORS_MESSAGE, "" } );
    //     return;
    // }
}

void DllPaletteControls::mouseEvent ( int x, int y, bool isDown, bool pressed, bool released )
{
    DllOverlayUi::paletteMouseEvent ( x, y, isDown, pressed, released );
}
