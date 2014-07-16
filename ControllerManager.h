#pragma once

#include "Controller.h"

#include <unordered_map>
#include <memory>

class ControllerManager
{
public:

    struct Owner
    {
        inline void attachedJoystick ( Controller *controller ) {}
        inline void detachedJoystick ( Controller *controller ) {}
    };

    Owner *owner;

private:

    // Keyboard controller instance
    Controller keyboard;

    // Maps of joystick controller instances
    std::unordered_map<int, std::shared_ptr<Controller>> joysticks;

    // Flag to inidicate if initialized
    bool initialized;

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

public:

    // Check for controller events
    void check();

    // Clear controllers
    void clear();

    // Initialize / deinitialize controller manager
    void initialize ( Owner *owner );
    void deinitialize();
    inline bool isInitialized() const { return initialized; }

    // Get the singleton instance
    static ControllerManager& get();
};
