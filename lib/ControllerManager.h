#pragma once

#include "Controller.h"
#include "Logger.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>


struct ControllerMappings : public SerializableSequence
{
    std::unordered_map<std::string, MsgPtr> mappings;

    DECLARE_MESSAGE_BOILERPLATE ( ControllerMappings )
};


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

    // All controllers mappings
    ControllerMappings mappings;

    // Keyboard controller instance
    Controller keyboard;

    // Maps of joystick controller instances
    std::unordered_map<int, std::shared_ptr<Controller>> joysticks;
    std::unordered_map<std::string, Controller *> joysticksByName;

    // Set of unique joystick names, used to workaround SDL bug 2643
    std::unordered_set<std::string> uniqueNames;

    // Flag to reinitialize joysticks, used to workaround SDL bug 2643
    bool shouldReset = false;

    // Flag to indicate if initialized
    bool initialized = false;

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

    // Check for joystick events
    void checkJoystick();

public:

    // Function that maps each controllers input state to a different representation
    static uint32_t ( *mapInputState ) ( uint32_t state, bool isKeyboard );

    // Check for controller events, matching keyboardWindowHandle if non-zero
    void check ( void *keyboardWindowHandle = 0 );

    // Clear controllers
    void clear();

    // Get the keyboard controller
    Controller *getKeyboard() { return &keyboard; }
    const Controller *getKeyboard() const { return &keyboard; }

    // Get the list of joysticks sorted by name
    std::vector<Controller *> getJoysticks();
    std::vector<const Controller *> getJoysticks() const;

    // Get all the controllers, sorted by name, except the keyboard is first
    std::vector<Controller *> getControllers();
    std::vector<const Controller *> getControllers() const;

    // Get the mappings for all controllers
    MsgPtr getMappings() const { return MsgPtr ( const_cast<ControllerMappings *> ( &mappings ), ignoreMsgPtr ); }

    // Set the mappings for all controllers
    void setMappings ( const ControllerMappings& mappings )
    {
        this->mappings = mappings;

        for ( auto& kv : mappings.mappings )
        {
            LOG ( "name=%s", kv.first );

            if ( kv.second->getMsgType() == MsgType::KeyboardMappings )
                keyboard.setMappings ( kv.second->getAs<KeyboardMappings>() );
            else if ( joysticksByName.find ( kv.first ) != joysticksByName.end() )
                joysticksByName[kv.first]->setMappings ( kv.second->getAs<JoystickMappings>() );
        }
    }

    // Indicate the mappings changed for a specific controller
    void mappingsChanged ( Controller *controller );

    // Initialize / deinitialize controller manager
    void initialize ( Owner *owner );
    void deinitialize();
    bool isInitialized() const { return initialized; }

    // Get the singleton instance
    static ControllerManager& get();
};
