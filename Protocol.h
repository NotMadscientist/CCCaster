#pragma once

#include <cereal/archives/binary.hpp>

#include <string>
#include <memory>

struct MsgType
{
    enum Enum
    {
#include "Protocol.enum.h"
    };

    const Enum value;

    MsgType ( Enum value ) : value ( value ) {};

    const char *c_str() const;
};

inline bool operator== ( const MsgType& a, const MsgType& b )
{
    return ( a.value == b.value );
}

inline bool operator!= ( const MsgType& a, const MsgType& b )
{
    return ! ( a == b );
}

struct Serializable;

typedef std::shared_ptr<Serializable> MsgPtr;

struct Serializable
{
    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;
    virtual const MsgType& type() const = 0;

    static std::string encode ( const Serializable& msg );
    static MsgPtr decode ( char *bytes, size_t len );
};
