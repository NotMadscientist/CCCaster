#pragma once

#include <cereal/archives/binary.hpp>

#include <string>
#include <memory>
#include <iostream>

// Increase size as needed
enum class MsgType : uint8_t
{
#include "Protocol.enum.h"
};

std::ostream& operator<< ( std::ostream& os, const MsgType& type );

enum class BaseType : uint8_t { SerializableMessage, SerializableSequence };

struct Serializable;
struct SerializableMessage;
struct SerializableSequence;

typedef std::shared_ptr<Serializable> MsgPtr;

struct Serializable
{
    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;
    virtual MsgType type() const = 0;
    virtual BaseType base() const = 0;

    static std::string encode ( const Serializable& msg );
    static MsgPtr decode ( char *bytes, size_t len );
};

struct SerializableMessage : public Serializable
{
    BaseType base() const { return BaseType::SerializableMessage; }
};

struct SerializableSequence : public Serializable
{
    uint32_t sequence;

    SerializableSequence() : sequence ( 0 ) {}
    SerializableSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    BaseType base() const { return BaseType::SerializableSequence; }
};
