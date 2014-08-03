#pragma once

#include <unordered_set>


class Socket;


class SocketManager
{
    // Sets of active and allocated socket instances
    std::unordered_set<Socket *> activeSockets, allocatedSockets;

    // Flag to indicate the set of allocated sockets has changed
    bool changed;

    // Flag to inidicate if initialized
    bool initialized;

    // Private constructor, etc. for singleton class
    SocketManager();
    SocketManager ( const SocketManager& );
    const SocketManager& operator= ( const SocketManager& );

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
    inline bool isInitialized() const { return initialized; }

    // Check if a socket is still allocated
    inline bool isAllocated ( Socket *socket ) const
    {
        return ( allocatedSockets.find ( socket ) != allocatedSockets.end() );
    }

    // Get the singleton instance
    static SocketManager& get();
};
