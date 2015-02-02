#include "StringUtils.h"

#include <windows.h>

using namespace std;


#define UNZIP "cccaster\\unzip.exe -o "


int main ( int argc, char *argv[] )
{
    if ( argc < 3 )
    {
        PRINT ( "Not enough arguments!" );
        system ( "pause" );
        return -1;
    }

    const string binary = argv[1];

    const string archive = argv[2];

    string appDir;
    for ( int i = 3; i < argc; ++i )
        appDir += string ( i == 3 ? "" : " " ) + argv[i];

    if ( appDir.empty() )
    {
        PRINT ( "Path argument is empty!" );
        system ( "pause" );
        return -1;
    }

    Sleep ( 5000 );

    SetCurrentDirectory ( appDir.c_str() );

    system ( ( UNZIP + archive ).c_str() );

    system ( ( "start \"\" " + binary ).c_str() );

    return 0;
}
