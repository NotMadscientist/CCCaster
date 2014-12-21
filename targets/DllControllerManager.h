#pragma once

#include "KeyboardManager.h"
#include "ControllerManager.h"
#include "Controller.h"
#include "DllControllerUtils.h"

#include <vector>
#include <array>


struct DllControllerManager
        : public KeyboardManager::Owner
        , public ControllerManager::Owner
        , public Controller::Owner
        , protected DllControllerUtils
{
    uint8_t localPlayer = 1, remotePlayer = 2;

    std::vector<Controller *> allControllers;

    std::array<Controller *, 2> playerControllers = {{ 0, 0 }};

    std::array<size_t, 2> overlayPositions = {{ 0, 0 }};

    std::array<bool, 2> finishedMapping = {{ false, false }};

    bool isSinglePlayer = false;


    void checkOverlay ( bool allowStartButton );

    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown ) override;

    void attachedJoystick ( Controller *controller ) override;

    void detachedJoystick ( Controller *controller ) override;

    void doneMapping ( Controller *controller, uint32_t key ) override;

    virtual void saveMappings ( const Controller *controller ) const = 0;
};
