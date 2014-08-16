#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <type_traits>
#include <cstdio>
#include <iostream>
#include <unordered_map>

#include <cereal/archives/binary.hpp>


#define TO_C_STR(...) toString ( __VA_ARGS__ ).c_str()

#define PRINT(...) do { std::cout << toString ( __VA_ARGS__ ) << std::endl; } while ( 0 )

#define ENUM(NAME, ...)                                                                                         \
    struct NAME : public BetterEnum {                                                                           \
        enum Enum : uint8_t { Unknown = 0, __VA_ARGS__ } value = Unknown;                                       \
        NAME() {}                                                                                               \
        NAME ( Enum value ) : value ( value ) {}                                                                \
        NAME& operator= ( Enum value ) { this->value = value; return *this; }                                   \
        std::string str() const override {                                                                      \
            static const std::vector<std::string> list = split ( "Unknown, " #__VA_ARGS__, ", " );              \
            return #NAME "::" + list[value];                                                                    \
        }                                                                                                       \
        void save ( cereal::BinaryOutputArchive& ar ) const override { ar ( value ); }                          \
        void load ( cereal::BinaryInputArchive& ar ) override { ar ( value ); }                                 \
        bool operator== ( const NAME& other ) const { return value == other.value; }                            \
        bool operator!= ( const NAME& other ) const { return value != other.value; }                            \
        bool operator== ( Enum other ) const { return value == other; }                                         \
        bool operator!= ( Enum other ) const { return value != other; }                                         \
    }


// Better enum type with autogen strings
struct BetterEnum
{
    virtual std::string str() const = 0;
    virtual void save ( cereal::BinaryOutputArchive& ar ) const = 0;
    virtual void load ( cereal::BinaryInputArchive& ar ) = 0;
};


inline std::ostream& operator<< ( std::ostream& os, const BetterEnum& value ) { return ( os << value.str() ); }


namespace std
{

// Hash util
template<class T>
void hash_combine ( size_t& seed, const T& v )
{
    hash<T> hasher;
    seed ^= hasher ( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

} // namespace std


// Convert a boolean value to a type
template<bool> struct bool2type {};

// Split a format string into first parameter and rest of string
void splitFormat ( const std::string& format, std::string& first, std::string& rest );

// Format bytes as a base 64 string
std::string toBase64 ( const std::string& bytes );
std::string toBase64 ( const void *bytes, size_t len );

// String trim
std::string trim ( std::string str, const std::string& ws = " \t\r\n" );

// String split
std::vector<std::string> split ( const std::string& str, const std::string& delim = " " );


// String formatting functions
template<typename T>
inline std::string toString ( const T& val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

template<>
inline std::string toString<std::string> ( const std::string& val ) { return val; }

template<>
inline std::string toString<BetterEnum> ( const BetterEnum& val ) { return val.str(); }

template<typename T>
void printToString ( char *buffer, size_t len, const char *format, const T& val, bool2type<false> ) // non-integer types
{
    std::snprintf ( buffer, len, format, TO_C_STR ( val ) );
}

template<typename T>
void printToString ( char *buffer, size_t len, const char *format, const T& val, bool2type<true> ) // integer types
{
    std::snprintf ( buffer, len, format, val );
}

template<typename T, typename ... V>
inline std::string toString ( const std::string& format, const T& val, V ... vals )
{
    std::string first, rest;
    splitFormat ( format, first, rest );

    if ( first.empty() )
        return rest;

    char buffer[4096];
    printToString ( buffer, sizeof ( buffer ), first.c_str(), val,
                    bool2type < std::is_arithmetic<T>::value || std::is_pointer<T>::value > () );

    if ( rest.empty() )
        return buffer;

    return buffer + toString ( rest, vals... );
}


// MD5 calculation
void getMD5 ( const char *bytes, size_t len, char dst[16] );
void getMD5 ( const std::string& str, char dst[16] );
bool checkMD5 ( const char *bytes, size_t len, const char md5[16] );
bool checkMD5 ( const std::string& str, const char md5[16] );


// zlib compression
size_t compress ( const char *src, size_t srcLen, char *dst, size_t dstLen, int level = 9 );
size_t uncompress ( const char *src, size_t srcLen, char *dst, size_t dstLen );
size_t compressBound ( size_t srcLen );


// General exception type
struct Exception
{
    std::string msg;

    Exception() {}
    Exception ( const std::string& msg ) : msg ( msg ) {}
};


// Windows exception type
struct WindowsException : public Exception
{
    int code = 0;

    WindowsException() {}
    WindowsException ( int code );
};


// Stream operators
std::ostream& operator<< ( std::ostream& os, const Exception& exception );
std::ostream& operator<< ( std::ostream& os, const WindowsException& error );


// Find the first window handle with the given title (NOT thread safe)
void *enumFindWindow ( const std::string& title );

// Detect if we're running on Wine
bool detectWine();


// Template class to calculate rolling averages
template<typename T>
class RollingAverage
{
    std::vector<T> values;
    T sum = 0, average = 0;
    size_t index = 0, count = 0;

public:

    RollingAverage ( size_t size ) : values ( size ) {}

    RollingAverage ( size_t size, T initial )
        : values ( size, initial ), sum ( initial ), average ( initial ), index ( 1 ), count ( 1 ) {}

    void set ( T value )
    {
        sum += value;

        if ( count < values.size() )
            ++count;
        else
            sum -= values[index];

        values[index] = value;

        index = ( index + 1 ) % values.size();

        average = sum / count;
    }

    T get() const
    {
        return average;
    }

    void reset()
    {
        sum = average = index = count = 0;
    }

    void reset ( T initial )
    {
        values[0] = initial;
        sum = average = initial;
        index = count = 1;
    }

    size_t size() const
    {
        return values.size();
    }
};


// Clamp a value to a range
template<typename T>
inline void clamp ( T& value, T min, T max )
{
    if ( value < min )
        value = min;
    else if ( value > max )
        value = max;
}


// Class to store configuration settings
class ConfigSettings
{
    enum class Type : uint8_t { String, Integer };
    std::unordered_map<std::string, std::string> settings;
    std::unordered_map<std::string, Type> types;

public:
    std::string getString ( const std::string& key );
    void putString ( const std::string& key, const std::string& str );

    int getInt ( const std::string& key );
    void putInt ( const std::string& key, int i );

    bool save ( const char *file ) const;
    bool load ( const char *file );
};
