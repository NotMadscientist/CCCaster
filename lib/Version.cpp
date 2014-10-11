#include "Version.h"

using namespace std;


string Version::get ( PartEnum part ) const
{
    size_t i = code.find ( '.' );

    if ( part == Major )
        return code.substr ( 0, i );

    if ( i == string::npos )
        return "";

    size_t j = code.find_first_not_of ( "0123456789", i + 1 );

    if ( part == Minor )
        return code.substr ( i + 1, j - ( i + 1 ) );

    if ( j == string::npos )
        return "";

    return code.substr ( j );
}

bool Version::similar ( const Version& other, uint8_t level ) const
{
    switch ( level )
    {
        default:
        case 4:
            if ( buildTime != other.buildTime )
                return false;

        case 3:
            if ( commitId != other.commitId )
                return false;

        case 2:
            if ( suffix() != other.suffix() )
                return false;

        case 1:
            if ( minor() != other.minor() )
                return false;

        case 0:
            if ( major() != other.major() )
                return false;
            break;
    }

    return true;
}


#include "Version.local.h"
