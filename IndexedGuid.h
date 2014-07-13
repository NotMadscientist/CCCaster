#pragma once

#include "Util.h"

#include <cstdint>
#include <cstring>
#include <iostream>

// Guid with an extra index property
struct IndexedGuid
{
    uint8_t guid[16];
    uint32_t index;
};

// Hash function
namespace std
{

template<> struct hash<IndexedGuid>
{
    inline size_t operator() ( const IndexedGuid& a ) const
    {
        size_t seed = 0;

        for ( size_t i = 0; i < sizeof ( a.guid ); ++i )
            hash_combine ( seed, a.guid[i] );

        hash_combine ( seed, a.index );

        return seed;
    }
};

}

// Comparison operators
inline bool operator< ( const IndexedGuid& a, const IndexedGuid& b ) { return ( memcmp ( &a, &b, sizeof ( a ) ) < 0 ); }
inline bool operator== ( const IndexedGuid& a, const IndexedGuid& b ) { return ( !memcmp ( &a, &b, sizeof ( a ) ) ); }
inline bool operator!= ( const IndexedGuid& a, const IndexedGuid& b ) { return ! ( a == b ); }

// Stream operators
inline std::istream& operator>> ( std::istream& is, IndexedGuid& a )
{
    uint32_t tmp;

    for ( size_t i = 0; i < sizeof ( a.guid ); ++i )
    {
        if ( ! ( is >> std::hex >> tmp ) )
            break;
        else
            a.guid[i] = ( uint8_t ) tmp;
    }

    return ( is >> std::dec >> a.index );
}

inline std::ostream& operator<< ( std::ostream& os, const IndexedGuid& a )
{
    return ( os << toBase64 ( a.guid, sizeof ( a.guid ) ) << ' ' << a.index );
}
