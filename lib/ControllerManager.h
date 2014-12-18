#pragma once

#include "Controller.h"
#include "JoystickDetector.h"
#include "Guid.h"

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
    std::unordered_map<Guid, std::shared_ptr<Controller>> joysticks;
    std::unordered_map<std::string, Controller *> joysticksByName;

    // Flag to indicate if initialized
    bool initialized = false;

    // Attach / detach a joystick
    void attachJoystick ( const Guid& guid, const std::string& name );
    void detachJoystick ( const Guid& guid );

    // Private constructor, etc. for singleton class
    ControllerManager();
    ControllerManager ( const ControllerManager& );
    const ControllerManager& operator= ( const ControllerManager& );

public:

    // Check for controller events, matching keyboardWindowHandle if non-zero
    void check ( void *keyboardWindowHandle = 0 );

    // Refresh the list of joysticks, will attach / detach joysticks accordingly
    void refreshJoysticks();

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

    // Save / load mappings to / from a folder, returns the number of mappings saved / loaded
    size_t saveMappings ( const std::string& folder, const std::string& ext ) const;
    size_t loadMappings ( const std::string& folder, const std::string& ext );

    // Save / load mappings
    static bool saveMappings ( const std::string& file, const MsgPtr& mappings );
    static bool saveMappings ( const std::string& file, const KeyboardMappings& mappings );
    static bool saveMappings ( const std::string& file, const JoystickMappings& mappings );
    static MsgPtr loadMappings ( const std::string& file );

    // Get the singleton instance
    static ControllerManager& get();
};
