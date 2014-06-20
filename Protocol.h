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

struct Serializable;

typedef std::shared_ptr<Serializable> MsgPtr;

const MsgPtr NullMsg;

struct Serializable
{
    virtual MsgType type() const = 0;
    virtual BaseType base() const = 0;

    template<typename T> T& getAs() { return *static_cast<T *> ( this ); }
    template<typename T> const T& getAs() const { return *static_cast<const T *> ( this ); }

    static std::string encode ( const Serializable& msg );
    static MsgPtr decode ( char *bytes, size_t len );

protected:

    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;

private:

    virtual void serializeBase ( cereal::BinaryOutputArchive& ar ) const {};
    virtual void deserializeBase ( cereal::BinaryInputArchive& ar ) {};

    friend struct SerializableMessage;
    friend struct SerializableSequence;
};

struct SerializableMessage : public Serializable
{
    BaseType base() const { return BaseType::SerializableMessage; }
};

struct SerializableSequence : public Serializable
{
    mutable uint32_t sequence;

    SerializableSequence() : sequence ( 0 ) {}
    SerializableSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    BaseType base() const { return BaseType::SerializableSequence; }

private:

    void serializeBase ( cereal::BinaryOutputArchive& ar ) const { ar ( sequence ); };
    void deserializeBase ( cereal::BinaryInputArchive& ar ) { ar ( sequence ); };
};

std::ostream& operator<< ( std::ostream& os, const MsgPtr& msg );
std::ostream& operator<< ( std::ostream& os, MsgType type );
std::ostream& operator<< ( std::ostream& os, BaseType type );
