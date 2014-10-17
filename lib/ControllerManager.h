#pragma once

#include "Controller.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>


class ControllerManager
{
public:

    struct Owner
    {
        virtual void attachedJoystick ( Controller *controller ) = 0;
        virtual void detachedJoystick ( Controller *controller ) = 0;
    };

    Owner *owner = 0;

private:

    // Keyboard controller instance
    Controller keyboard;

    // Maps of joystick controller instances
    std::unordered_map<int, std::shared_ptr<Controller>> joysticks;

    // Set of unique joystick guids, used to workaround SDL bug 2643
    std::unordered_set<Guid> guids;

    // Flag to reinitialize joysticks, used to workaround SDL bug 2643
    bool shouldReset = false;

    // Flag to indicate if initialized
    bool initialized = false;

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

    // Check for controller events
    void doCheck();

public:

    // Check for controller events
    void check();

    // Clear controllers
    void clear();

    // Get the keyboard controller
    Controller *getKeyboard() { return &keyboard; }

    // Get the list of joysticks sorted by name
    std::vector<Controller *> getJoysticks();

    // Get all the controllers, sorted by name, except the keyboard is first
    std::vector<Controller *> getControllers();

    // Initialize / deinitialize controller manager
    void initialize ( Owner *owner );
    void deinitialize();
    bool isInitialized() const { return initialized; }

    // Get the singleton instance
    static ControllerManager& get();
};
