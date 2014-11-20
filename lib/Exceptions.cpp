#include "Exceptions.h"
#include "StringUtils.h"

#include <windows.h>

using namespace std;


static string getWindowsExceptionAsString ( int error )
{
    string str;
    char *errorString = 0;
    FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, error, 0, ( LPSTR ) &errorString, 0, 0 );
    str = ( errorString ? trim ( errorString ) : "(null)" );
    LocalFree ( errorString );
    return str;
}


string Exception::str() const { return msg; }


WindowsException::WindowsException ( int code )
    : Exception ( getWindowsExceptionAsString ( code ) )
    , code ( code ) {}

string WindowsException::str() const
{
    return format ( "[%d] '%s'", code, msg );
}


ostream& operator<< ( ostream& os, const Exception& error )
{
    return ( os << error.str() );
}
