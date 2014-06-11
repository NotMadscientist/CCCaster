#pragma once

#include <cereal/archives/binary.hpp>

#include <string>
#include <memory>

enum SerializableType
{
#include "Protocol.enum.h"
};

struct Serializable;

typedef std::shared_ptr<Serializable> MsgPtr;

struct Serializable
{
    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;
    virtual SerializableType type() const = 0;

    static std::string encode ( const Serializable& msg );
    static MsgPtr decode ( char *bytes, size_t len );
};
