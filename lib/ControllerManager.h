#pragma once

#include "Controller.h"

#include <unordered_map>
#include <memory>


class ControllerManager
{
public:

    struct Owner
    {
        virtual void attachedJoystick ( Controller *controller ) {}
        virtual void detachedJoystick ( Controller *controller ) {}
    };

    Owner *owner = 0;

private:

    // Keyboard controller instance
    Controller keyboard;

    // Maps of joystick controller instances
    std::unordered_map<int, std::shared_ptr<Controller>> joysticks;

    // Flag to inidicate if initialized
    bool initialized = false;

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

public:

    // Check for controller events
    void check();

    // Clear controllers
    void clear();

    // Get the keyboard controller
    Controller *getKeyboard() { return &keyboard; };

    // Initialize / deinitialize controller manager
    void initialize ( Owner *owner );
    void deinitialize();
    bool isInitialized() const { return initialized; }

    // Get the singleton instance
    static ControllerManager& get();
};
