#pragma once

#include "KeyboardManager.h"
#include "MouseManager.h"
#include "ControllerManager.h"
#include "Controller.h"
#include "DllControllerUtils.h"
#include "DllPaletteManager.h"

#include <vector>
#include <array>


class DllControllerManager
    : public KeyboardManager::Owner
    , public MouseManager::Owner
    , public ControllerManager::Owner
    , public Controller::Owner
    , protected DllControllerUtils
{
    std::vector<Controller *> allControllers;

    std::array<Controller *, 2> playerControllers = {{ 0, 0 }};

    std::array<size_t, 2> overlayPositions = {{ 0, 0 }};

    std::array<bool, 2> finishedMapping = {{ false, false }};

protected:

    std::unordered_map<uint32_t, DllPaletteManager> palMans;

public:

    // Local vs remote player numbers
    uint8_t localPlayer = 1, remotePlayer = 2;

    // Single player setting
    bool isSinglePlayer = false;

    // Initialize all controllers with the given mappings
    void initControllers ( const ControllerMappings& mappings );

    // True only if both controllers are not mapping
    bool isNotMapping() const;

    // Update local controls and overlay UI inputs
    void updateControls ( uint16_t *localInputs );

    // Update palette editor controls and state
    void updatePaletteEditor();

    // KeyboardManager callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown ) override;

    // MouseManager callback
    void mouseEvent ( int x, int y, bool isDown, bool pressed, bool released ) override;

    // ControllerManager callbacks
    void attachedJoystick ( Controller *controller ) override;
    void detachedJoystick ( Controller *controller ) override;

    // Controller callback
    void doneMapping ( Controller *controller, uint32_t key ) override;

    // To be implemented
    virtual void saveMappings ( const Controller *controller ) const = 0;
};
