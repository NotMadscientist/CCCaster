#pragma once

#include "IndexedGuid.h"

#include <unordered_map>
#include <memory>
#include <cstring>

class Controller;

class ControllerManager
{
public:

    struct Owner
    {
        inline void attachedController ( const IndexedGuid& guid, Controller *controller ) {}
        inline void detachedController ( const IndexedGuid& guid, Controller *controller ) {}
    };

    Owner *owner;

private:

    // Maps of active and allocated controller instances
    std::unordered_map<IndexedGuid, Controller *> activeControllers;
    std::unordered_map<IndexedGuid, std::shared_ptr<Controller>> allocatedControllers;

    // Flag to inidicate if initialized
    bool initialized;

    // Update the set of allocated controllers
    void updateControllers();

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

    // Get the set of active controllers
    inline std::unordered_map<IndexedGuid, Controller *> getControllers() const { return activeControllers; }

    // Get the singleton instance
    static ControllerManager& get();
};
