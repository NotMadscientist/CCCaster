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
    mutable uint8_t compressionLevel;

    Serializable();
    inline virtual ~Serializable() {}

    virtual MsgType getType() const = 0;
    virtual BaseType getBaseType() const = 0;

    template<typename T> T& getAs() { return *static_cast<T *> ( this ); }
    template<typename T> const T& getAs() const { return *static_cast<const T *> ( this ); }

    inline void invalidate() const { md5empty = true; }

    static std::string encode ( Serializable *message );
    static std::string encode ( const MsgPtr& msg );
    static MsgPtr decode ( const char *bytes, size_t len, size_t& consumed );

protected:

    virtual void serialize ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void deserialize ( cereal::BinaryInputArchive& ar ) = 0;

private:

    mutable char md5[16];
    mutable bool md5empty;

    inline virtual void serializeBase ( cereal::BinaryOutputArchive& ar ) const {};
    inline virtual void deserializeBase ( cereal::BinaryInputArchive& ar ) {};

    friend struct SerializableMessage;
    friend struct SerializableSequence;
};

struct SerializableMessage : public Serializable
{
    BaseType getBaseType() const override { return BaseType::SerializableMessage; }
};

struct SerializableSequence : public Serializable
{
public:

    SerializableSequence() : sequence ( 0 ) {}
    SerializableSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    BaseType getBaseType() const override { return BaseType::SerializableSequence; }

    inline uint32_t getSequence() const { return sequence; }
    inline void setSequence ( uint32_t sequence ) const { invalidate(); this->sequence = sequence; }

private:

    mutable uint32_t sequence;

    void serializeBase ( cereal::BinaryOutputArchive& ar ) const override { ar ( sequence ); };
    void deserializeBase ( cereal::BinaryInputArchive& ar ) override { ar ( sequence ); };
};

std::ostream& operator<< ( std::ostream& os, const MsgPtr& msg );
std::ostream& operator<< ( std::ostream& os, MsgType type );
std::ostream& operator<< ( std::ostream& os, BaseType type );
