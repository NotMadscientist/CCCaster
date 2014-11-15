#pragma once

#include "Utilities.h"
#include "Protocol.h"

#include <cstdint>
#include <cstring>
#include <iostream>


// Basic guid type
struct Guid : public SerializableSequence
{
    uint8_t guid[16];

    PROTOCOL_MESSAGE_BOILERPLATE ( Guid, guid )
};


// Guid with an extra index property
struct IndexedGuid : public SerializableSequence
{
    Guid guid;
    uint8_t index = 0;

    PROTOCOL_MESSAGE_BOILERPLATE ( IndexedGuid, guid, index )
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


template<> struct hash<IndexedGuid>
{
    size_t operator() ( const IndexedGuid& a ) const
    {
        size_t seed = 0;

        for ( size_t i = 0; i < sizeof ( a.guid.guid ); ++i )
            hash_combine ( seed, a.guid.guid[i] );

        hash_combine ( seed, a.index );

        return seed;
    }
};

} // namespace std


// Comparison operators
inline bool operator< ( const Guid& a, const Guid& b ) { return ( memcmp ( &a, &b, sizeof ( a ) ) < 0 ); }
inline bool operator== ( const Guid& a, const Guid& b ) { return ( !memcmp ( &a, &b, sizeof ( a ) ) ); }
inline bool operator!= ( const Guid& a, const Guid& b ) { return ! ( a == b ); }
inline bool operator< ( const IndexedGuid& a, const IndexedGuid& b ) { return ( memcmp ( &a, &b, sizeof ( a ) ) < 0 ); }
inline bool operator== ( const IndexedGuid& a, const IndexedGuid& b ) { return ( !memcmp ( &a, &b, sizeof ( a ) ) ); }
inline bool operator!= ( const IndexedGuid& a, const IndexedGuid& b ) { return ! ( a == b ); }


// Stream operators
inline std::istream& operator>> ( std::istream& is, IndexedGuid& a )
{
    uint32_t tmp;

    for ( size_t i = 0; i < sizeof ( a.guid.guid ); ++i )
    {
        if ( ! ( is >> std::hex >> tmp ) )
            break;
        else
            a.guid.guid[i] = ( uint8_t ) tmp;
    }

    return ( is >> std::dec >> a.index );
}

inline std::ostream& operator<< ( std::ostream& os, const Guid& a )
{
    return ( os << toBase64 ( a.guid, sizeof ( a.guid ) ) );
}

inline std::ostream& operator<< ( std::ostream& os, const IndexedGuid& a )
{
    return ( os << toBase64 ( a.guid.guid, sizeof ( a.guid.guid ) ) << ' ' << ( uint32_t ) a.index );
}
