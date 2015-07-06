#pragma once


#define ERROR_INTERNAL              "Internal error!"

#define ERROR_INVALID_HOSTNAME      "Cannot find host: %s"

#define ERROR_NETWORK_INIT          "Cannot initialize networking!"

#define ERROR_NETWORK_GENERIC       "Network error!"

#define ERROR_NETWORK_PORT_BIND     "Port %u already being used!"

#define ERROR_TOO_MANY_CONTROLLERS  "Too many duplicate joysticks with the name: '%s'"

#define ERROR_CONTROLLER_INIT       "Failed to initialize controllers!"

#define ERROR_CONTROLLER_CHECK      "Failed to check controllers!"

#define ERROR_INVALID_PORT          "Port must be less than 65536!"

#define ERROR_INVALID_ADDR_PORT     "Invalid IP address and/or port!\n" ERROR_INVALID_PORT
