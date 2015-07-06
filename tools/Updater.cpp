#include "StringUtils.hpp"

#include <cstdlib>

#include <windows.h>
#include <psapi.h>

using namespace std;


#define UNZIP "cccaster\\unzip.exe -o "


int main ( int argc, char *argv[] )
{
    if ( argc < 4 )
    {
        PRINT ( "Not enough arguments!" );
        system ( "pause" );
        return -1;
    }

    const DWORD processId = atoi ( argv[1] );

    const string binary = argv[2];

    const string archive = argv[3];

    string appDir;
    for ( int i = 4; i < argc; ++i )
        appDir += string ( i == 4 ? "" : " " ) + argv[i];

    if ( appDir.empty() )
    {
        PRINT ( "Path argument is empty!" );
        system ( "pause" );
        return -1;
    }

    HANDLE process = OpenProcess ( PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, 0, processId );

    if ( process )
    {
        char buffer[4096];

        if ( !GetProcessImageFileName ( process, buffer, sizeof ( buffer ) ) )
            buffer[0] = '\0';

        if ( string ( buffer ).find ( "cccaster" ) != string::npos )
        {
            TerminateProcess ( process, 9 );
            CloseHandle ( process );
        }
    }

    PRINT ( "Updating...\n" );

    SetCurrentDirectory ( appDir.c_str() );

    system ( ( UNZIP + archive ).c_str() );

    system ( ( "start \"\" " + binary ).c_str() );

    // system ( "pause" );

    return 0;
}
