#pragma once

#include "StringUtils.h"

#include <vector>
#include <string>

#include <cereal/archives/binary.hpp>


// Enum type boilerplate code
#define ENUM_BOILERPLATE(NAME, ...)                                                                             \
    enum Enum : uint8_t { Unknown = 0, __VA_ARGS__ } value = Unknown;                                           \
    NAME ( Enum value ) : value ( value ) {}                                                                    \
    std::string str() const override {                                                                          \
        static const std::vector<std::string> list = split ( "Unknown, " #__VA_ARGS__, ", " );                  \
        if ( value >= list.size() )                                                                             \
            return format ( "Unknown (%u)", value );                                                            \
        return #NAME "::" + list[value];                                                                        \
    }                                                                                                           \
    bool operator== ( const NAME& other ) const { return value == other.value; }                                \
    bool operator!= ( const NAME& other ) const { return value != other.value; }                                \
    bool operator== ( Enum other ) const { return value == other; }                                             \
    bool operator!= ( Enum other ) const { return value != other; }


// Enum type with auto-generated string values
#define ENUM(NAME, ...)                                                                                         \
    struct NAME : public EnumBase {                                                                             \
        ENUM_BOILERPLATE ( NAME, __VA_ARGS__ )                                                                  \
        NAME() {}                                                                                               \
        NAME& operator= ( Enum value ) { this->value = value; return *this; }                                   \
        void save ( cereal::BinaryOutputArchive& ar ) const { ar ( value ); }                                   \
        void load ( cereal::BinaryInputArchive& ar ) { ar ( value ); }                                          \
    } //


// Enum base class
struct EnumBase
{
    virtual std::string str() const = 0;
    virtual void save ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void load ( cereal::BinaryInputArchive& ar ) = 0;
};


// Stream operator
inline std::ostream& operator<< ( std::ostream& os, const EnumBase& value ) { return ( os << value.str() ); }


// Specialize format template function
template<>
inline std::string format<EnumBase> ( const EnumBase& val ) { return val.str(); }
