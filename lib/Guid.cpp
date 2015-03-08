#include "Guid.h"
#include "Logger.h"

#include <algorithm>

#include <rpc.h>

using namespace std;


Guid::Guid ( std::initializer_list<uint8_t> guid )
{
    ASSERT ( guid.size() == sizeof ( this->guid ) );

    copy ( guid.begin(), guid.end(), &this->guid[0] );
}

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
