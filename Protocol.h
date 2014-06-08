#pragma once

#include <cereal/archives/binary.hpp>

#include <string>
#include <memory>

enum SerializableType
{
#include "Protocol.enum.h"
};

struct Serializable
{
    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;
    virtual SerializableType type() const = 0;

    static std::string encode ( const Serializable& msg );
    static std::shared_ptr<Serializable> decode ( char *bytes, std::size_t len );
};
