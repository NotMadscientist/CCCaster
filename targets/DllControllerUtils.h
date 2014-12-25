#pragma once

#include "ProcessManager.h"
#include "Controller.h"


struct DllControllerUtils
{
    // Filter simultaneous up / down and left / right directions.
    // Prioritize down and left for keyboard only.
    static uint16_t filterSimulDirState ( uint16_t state, bool isKeyboard )
    {
        if ( isKeyboard )
        {
            if ( ( state & ( BIT_UP | BIT_DOWN ) ) == ( BIT_UP | BIT_DOWN ) )
                state &= ~BIT_UP;
            if ( ( state & ( BIT_LEFT | BIT_RIGHT ) ) == ( BIT_LEFT | BIT_RIGHT ) )
                state &= ~BIT_RIGHT;
        }
        else
        {
            if ( ( state & ( BIT_UP | BIT_DOWN ) ) == ( BIT_UP | BIT_DOWN ) )
                state &= ~ ( BIT_UP | BIT_DOWN );
            if ( ( state & ( BIT_LEFT | BIT_RIGHT ) ) == ( BIT_LEFT | BIT_RIGHT ) )
                state &= ~ ( BIT_LEFT | BIT_RIGHT );
        }

        return state;
    }

    static uint16_t convertInputState ( uint32_t state, bool isKeyboard )
    {
        const uint16_t dirs = filterSimulDirState ( state & MASK_DIRS, isKeyboard );
        const uint16_t buttons = ( state & MASK_BUTTONS ) >> 8;

        uint8_t direction = 5;

        if ( dirs & BIT_UP )
            direction = 8;
        else if ( dirs & BIT_DOWN )
            direction = 2;

        if ( dirs & BIT_LEFT )
            --direction;
        else if ( dirs & BIT_RIGHT )
            ++direction;

        if ( direction == 5 )
            direction = 0;

        return COMBINE_INPUT ( direction, buttons );
    }

    static uint16_t getPrevInput ( const Controller *controller )
    {
        if ( !controller )
            return 0;

        return convertInputState ( controller->getPrevState(), controller->isKeyboard() );
    }

    static uint16_t getInput ( const Controller *controller )
    {
        if ( !controller )
            return 0;

        return convertInputState ( controller->getState(), controller->isKeyboard() );
    }

    static bool isButtonPressed ( const Controller *controller, uint16_t button )
    {
        if ( !controller )
            return false;

        button = COMBINE_INPUT ( 0, button );
        return ( getInput ( controller ) & button ) && ! ( getPrevInput ( controller ) & button );
    }

    static bool isDirectionPressed ( const Controller *controller, uint16_t dir )
    {
        if ( !controller )
            return false;

        return ( ( getInput ( controller ) & MASK_DIRS ) == dir )
               && ( ( getPrevInput ( controller ) & MASK_DIRS ) != dir );
    }

    static bool isAnyDirectionPressed ( const Controller *controller )
    {
        if ( !controller )
            return false;

        return ( getInput ( controller ) & MASK_DIRS ) && ! ( getPrevInput ( controller ) & MASK_DIRS );
    }

    static bool isAnyButtonPressed ( const Controller *controller )
    {
        if ( !controller )
            return false;

        return ( controller->getAnyButton() && !controller->getPrevAnyButton() );
    }
};
