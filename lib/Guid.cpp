#include "Guid.h"

#include <rpc.h>

using namespace std;


Guid::Guid ( const GUID& guid )
{
    static_assert ( sizeof ( guid ) == sizeof ( this->guid ), "Must be the same size as Windows GUID" );

    memcpy ( &this->guid[0], &guid, sizeof ( this->guid ) );
}

void Guid::getGUID ( GUID& guid ) const
{
    static_assert ( sizeof ( guid ) == sizeof ( this->guid ), "Must be the same size as Windows GUID" );

    memcpy ( &guid, &this->guid[0], sizeof ( this->guid ) );
}
