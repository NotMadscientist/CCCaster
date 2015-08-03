#pragma once

#include <unordered_set>


class Socket;


class SocketManager
{
public:

    // Check for socket events
    void check ( uint64_t timeout );

    // Add / remove / clear socket instances
    void add ( Socket *socket );
    void remove ( Socket *socket );
    void clear();

    // Initialize / deinitialize socket manager
    void initialize();
    void deinitialize();
    bool isInitialized() const { return _initialized; }

    // Check if a socket is still allocated
    bool isAllocated ( Socket *socket ) const
    {
        return ( _allocatedSockets.find ( socket ) != _allocatedSockets.end() );
    }

    // Get the singleton instance
    static SocketManager& get();

private:

    // Sets of active and allocated socket instances
    std::unordered_set<Socket *> _activeSockets, _allocatedSockets;

    // Flag to indicate the set of allocated sockets has changed
    bool _changed = false;

    // Flag to indicate if initialized
    bool _initialized = false;

    // Private constructor, etc. for singleton class
    SocketManager();
    SocketManager ( const SocketManager& );
    const SocketManager& operator= ( const SocketManager& );
};
