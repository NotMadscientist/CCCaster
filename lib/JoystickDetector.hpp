#pragma once

#include "Protocol.hpp"
#include "Socket.hpp"


struct JoysticksChanged : public SerializableMessage
{
    EMPTY_MESSAGE_BOILERPLATE ( JoysticksChanged )
};


// Class that detects joystick attach / detach events, and automatically updates ControllerManager
class JoystickDetector : private Socket::Owner
{
public:

    // Start detecting joysticks
    void start();

    // Stop detecting joysticks
    void stop();

    // Get the singleton instance
    static JoystickDetector& get();

private:

    // Socket callbacks
    void acceptEvent ( Socket *socket ) override {}
    void connectEvent ( Socket *socket ) override {}
    void disconnectEvent ( Socket *socket ) override {}
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override;
};
