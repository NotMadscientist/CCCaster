#include "Event.h"
#include "Socket.h"
#include "Log.h"

#include <winsock2.h>
#include <windows.h>

#define SELECT_TIMEOUT_MICROSECONDS 100

using namespace std;

void EventManager::checkSockets()
{
    for ( Socket *socket : allocatedSockets )
    {
        if ( activeSockets.find ( socket ) != activeSockets.end() )
            continue;

        LOG_SOCKET ( socket, "added" );
        activeSockets.insert ( socket );
    }

    for ( auto it = activeSockets.begin(); it != activeSockets.end(); )
    {
        if ( allocatedSockets.find ( *it ) != allocatedSockets.end() )
        {
            ++it;
            continue;
        }

        LOG ( "socket %08x removed", *it ); // Don't log any extra data cus already deleted
        activeSockets.erase ( it++ );
    }

    if ( activeSockets.empty() )
        return;

    fd_set readFds, writeFds;
    FD_ZERO ( &readFds );
    FD_ZERO ( &writeFds );

    for ( Socket *socket : activeSockets )
    {
        if ( socket->isConnecting() && socket->isTCP() )
            FD_SET ( socket->fd, &writeFds );
        else
            FD_SET ( socket->fd, &readFds );
    }

    timeval timeout;
    timeout.tv_sec = SELECT_TIMEOUT_MICROSECONDS / ( 1000 * 1000 );
    timeout.tv_usec = SELECT_TIMEOUT_MICROSECONDS % ( 1000 * 1000 );

    int count = select ( 0, &readFds, &writeFds, 0, &timeout );

    if ( count == SOCKET_ERROR )
    {
        WindowsError err = WSAGetLastError();
        LOG ( "select failed: %s", err );
        throw err;
    }

    if ( count == 0 )
        return;

    for ( Socket *socket : activeSockets )
    {
        if ( allocatedSockets.find ( socket ) == allocatedSockets.end() )
            continue;

        if ( socket->isConnecting() && socket->isTCP() )
        {
            if ( !FD_ISSET ( socket->fd, &writeFds ) )
                continue;

            LOG_SOCKET ( socket, "connectEvent" );
            socket->connectEvent();
        }
        else
        {
            if ( !FD_ISSET ( socket->fd, &readFds ) )
                continue;

            if ( socket->isServer() && socket->isTCP() )
            {
                LOG_SOCKET ( socket, "acceptEvent" );
                socket->acceptEvent();
            }
            else
            {
                u_long numBytes;

                if ( ioctlsocket ( socket->fd, FIONREAD, &numBytes ) != 0 )
                {
                    WindowsError err = WSAGetLastError();
                    LOG ( "ioctlsocket failed: %s", err );
                    throw err;
                }

                if ( socket->isTCP() && numBytes == 0 )
                {
                    LOG_SOCKET ( socket, "disconnectEvent" );
                    socket->disconnectEvent();
                }
                else
                {
                    LOG_SOCKET ( socket, "readEvent" );
                    socket->readEvent();
                }
            }
        }
    }
}

void EventManager::addSocket ( Socket *socket )
{
    LOG_SOCKET ( socket, "addSocket" );
    allocatedSockets.insert ( socket );
}

void EventManager::removeSocket ( Socket *socket )
{
    LOG_SOCKET ( socket, "removeSocket" );
    allocatedSockets.erase ( socket );
}

void EventManager::clearSockets()
{
    LOG ( "clearSockets" );
    activeSockets.clear();
    allocatedSockets.clear();
}

void EventManager::initializeSockets()
{
    if ( initializedSockets )
        return;

    // Initialize WinSock
    WSADATA wsaData;
    int error = WSAStartup ( MAKEWORD ( 2, 2 ), &wsaData );

    if ( error != NO_ERROR )
    {
        WindowsError err = error;
        LOG ( "WSAStartup failed: %s", err );
        throw err;
    }

    initializedSockets = true;
}

void EventManager::deinitializeSockets()
{
    if ( !initializedSockets )
        return;

    WSACleanup();
    initializedSockets = false;
}
