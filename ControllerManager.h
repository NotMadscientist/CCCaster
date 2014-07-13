#pragma once

#include <unordered_set>

class ControllerManager
{
    // Flag to inidicate if initialized
    bool initialized;

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

public:

    // Check for timer events
    void check();

    // Clear joysticks
    void clear();

    // Initialize / deinitialize joystick manager
    void initialize();
    void deinitialize();

    // Get the singleton instance
    static ControllerManager& get();
};
