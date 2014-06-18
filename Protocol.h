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

enum class BaseType : uint8_t { SerializableMessage, SerializableSequence };

class Serializable;
class SerializableMessage;
class SerializableSequence;

typedef std::shared_ptr<Serializable> MsgPtr;

const MsgPtr NullMsg;

class Serializable
{
    virtual void serializeBase ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserializeBase ( cereal::BinaryInputArchive& ar ) = 0;

protected:

    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;

public:

    virtual MsgType type() const = 0;
    virtual BaseType base() const = 0;

    static std::string encode ( const Serializable& msg );
    static MsgPtr decode ( char *bytes, size_t len );

    friend class SerializableMessage;
    friend class SerializableSequence;
};

class SerializableMessage : public Serializable
{
    void serializeBase ( cereal::BinaryOutputArchive& ar ) const { serialize ( ar ); };
    void deserializeBase ( cereal::BinaryInputArchive& ar ) { deserialize ( ar ); };

public:

    BaseType base() const { return BaseType::SerializableMessage; }
};

class SerializableSequence : public Serializable
{
    void serializeBase ( cereal::BinaryOutputArchive& ar ) const { ar ( sequence ); serialize ( ar ); };
    void deserializeBase ( cereal::BinaryInputArchive& ar ) { ar ( sequence ); deserialize ( ar ); };

public:

    uint32_t sequence;

    SerializableSequence() : sequence ( 0 ) {}
    SerializableSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    BaseType base() const { return BaseType::SerializableSequence; }
};

std::ostream& operator<< ( std::ostream& os, const MsgPtr& msg );
std::ostream& operator<< ( std::ostream& os, MsgType type );
std::ostream& operator<< ( std::ostream& os, BaseType type );
