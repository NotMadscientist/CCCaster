#pragma once

#include <unordered_set>

class JoystickManager
{
    // Flag to inidicate if initialized
    bool initialized;

    // Private constructor, etc. for singleton class
    JoystickManager();
    JoystickManager ( const JoystickManager& );
    const JoystickManager& operator= ( const JoystickManager& );

public:

    // Check for timer events
    void check();

    // Clear joysticks
    void clear();

    // Initialize / deinitialize joystick manager
    void initialize();
    void deinitialize();

    // Get the singleton instance
    static JoystickManager& get();
};
