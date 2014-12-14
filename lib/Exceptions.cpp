#include "Exceptions.h"
#include "StringUtils.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

using namespace std;


string Exception::str() const
{
    if ( debug.empty() || debug == user )
        return user;

    if ( user.empty() )
        return debug;

    return debug + "; " + user;
}

string WinException::str() const
{
    return format ( "[%d] '%s'; %s; %s", code, desc, debug, user );
}

string WinException::getAsString ( int windowsErrorCode )
{
    string str;
    char *errorString = 0;
    FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, windowsErrorCode, 0, ( LPSTR ) &errorString, 0, 0 );
    str = ( errorString ? trimmed ( errorString ) : "(null)" );
    LocalFree ( errorString );
    return str;
}

string WinException::getLastError()
{
    return getAsString ( GetLastError() );
}

string WinException::getLastSocketError()
{
    return getAsString ( WSAGetLastError() );
}
