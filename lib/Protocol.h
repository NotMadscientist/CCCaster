#pragma once

#include <cereal/archives/binary.hpp>

#include <string>
#include <memory>
#include <iostream>
#include <sstream>


#define PROTOCOL_BOILERPLATE(...)                                                                           \
    MsgType getMsgType() const override;                                                                    \
    void save ( cereal::BinaryOutputArchive& ar ) const override { ar ( __VA_ARGS__ ); }                    \
    void load ( cereal::BinaryInputArchive& ar ) override { ar ( __VA_ARGS__ ); }

#define EMPTY_MESSAGE_BOILERPLATE(NAME)                                                                     \
    inline NAME() {}                                                                                        \
    MsgType getMsgType() const override;

#define ENUM_MESSAGE_BOILERPLATE(NAME, ...)                                                                 \
    enum Enum : uint8_t { Unknown, __VA_ARGS__ } value;                                                     \
    inline NAME() : value ( Unknown ) {}                                                                    \
    inline NAME ( Enum value ) : value ( value ) {}                                                         \
    std::string str() const override {                                                                      \
        static const std::vector<std::string> list = split ( "Unknown, " #__VA_ARGS__, ", " );              \
        return #NAME "::" + list[value];                                                                    \
    }                                                                                                       \
    PROTOCOL_BOILERPLATE ( value )


// Increase size as needed
enum class MsgType : uint8_t
{
#include "Protocol.enum.h"
};

enum class BaseType : uint8_t { SerializableMessage, SerializableSequence };

struct Serializable;
typedef std::shared_ptr<Serializable> MsgPtr;

const MsgPtr NullMsg;


// Stream operators
std::ostream& operator<< ( std::ostream& os, MsgType type );
std::ostream& operator<< ( std::ostream& os, BaseType type );
std::ostream& operator<< ( std::ostream& os, const MsgPtr& msg );


// Abstract base class for all serializable messages
struct Serializable
{
    // Flag to indicate compression level
    mutable uint8_t compressionLevel;

    // Basic constructor and destructor
    Serializable();
    inline virtual ~Serializable() {}

    // Get message and base types
    virtual MsgType getMsgType() const = 0;
    virtual BaseType getBaseType() const = 0;

    // Serialize to and deserialize from a binary archive
    inline virtual void save ( cereal::BinaryOutputArchive& ar ) const {};
    inline virtual void load ( cereal::BinaryInputArchive& ar ) {};

    // Cast this to another another type
    template<typename T> T& getAs() { return *static_cast<T *> ( this ); }
    template<typename T> const T& getAs() const { return *static_cast<const T *> ( this ); }

    // Invalidate and any cached data
    inline void invalidate() const { md5empty = true; }

    // Return a string representation of this message, defaults to the message type
    inline virtual std::string str() const { std::stringstream ss; ss << getMsgType(); return ss.str(); }

    // Encode and decode messages
    static std::string encode ( Serializable *message );
    static std::string encode ( const MsgPtr& msg );
    static MsgPtr decode ( const char *bytes, size_t len, size_t& consumed );

private:

    // Cached MD5 data
    mutable char md5[16];
    mutable bool md5empty;

    // Serialize and deserialize the base type
    inline virtual void saveBase ( cereal::BinaryOutputArchive& ar ) const {};
    inline virtual void loadBase ( cereal::BinaryInputArchive& ar ) {};

    friend struct SerializableMessage;
    friend struct SerializableSequence;
};


// Represents a regular message
struct SerializableMessage : public Serializable
{
    BaseType getBaseType() const override { return BaseType::SerializableMessage; }
};


// Represents a sequential message
struct SerializableSequence : public Serializable
{
    // Basic constructors
    SerializableSequence() : sequence ( 0 ) {}
    SerializableSequence ( uint32_t sequence ) : sequence ( sequence ) {}

    BaseType getBaseType() const override { return BaseType::SerializableSequence; }

    // Get and set the message sequence
    inline uint32_t getSequence() const { return sequence; }
    inline void setSequence ( uint32_t sequence ) const { invalidate(); this->sequence = sequence; }

private:

    // Message sequence number
    mutable uint32_t sequence;

    void saveBase ( cereal::BinaryOutputArchive& ar ) const override { ar ( sequence ); };
    void loadBase ( cereal::BinaryInputArchive& ar ) override { ar ( sequence ); };
};
