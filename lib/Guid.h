#pragma once

#include "Algorithms.h"
#include "StringUtils.h"

#include <cstring>


// Windows GUID type forward declaration
typedef struct _GUID GUID;


// Guid type
struct Guid
{
    uint8_t guid[16];

    Guid() {}

    Guid ( const GUID& guid );

    void getGUID ( GUID& guid ) const;
};


// Hash function
namespace std
{

template<> struct hash<Guid>
{
    size_t operator() ( const Guid& a ) const
    {
        size_t seed = 0;

        for ( size_t i = 0; i < sizeof ( a.guid ); ++i )
            hash_combine ( seed, a.guid[i] );

        return seed;
    }
};

} // namespace std


// Comparison operators
inline bool operator< ( const Guid& a, const Guid& b ) { return ( std::memcmp ( &a, &b, sizeof ( a ) ) < 0 ); }
inline bool operator== ( const Guid& a, const Guid& b ) { return ( !std::memcmp ( &a, &b, sizeof ( a ) ) ); }
inline bool operator!= ( const Guid& a, const Guid& b ) { return ! ( a == b ); }


// Stream operators
inline std::ostream& operator<< ( std::ostream& os, const Guid& a )
{
    return ( os << formatAsHex ( a.guid, sizeof ( a.guid ) ) );
}
