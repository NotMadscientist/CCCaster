#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <type_traits>
#include <cstdio>
#include <iostream>
#include <map>
#include <cctype>
#include <algorithm>

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
        bool operator== ( const NAME& other ) const { return value == other.value; }                            \
        bool operator!= ( const NAME& other ) const { return value != other.value; }                            \
        bool operator== ( Enum other ) const { return value == other; }                                         \
        bool operator!= ( Enum other ) const { return value != other; }                                         \
        CEREAL_CLASS_BOILERPLATE ( value )                                                                      \
    }

#define ENUM_VALUE(NAME, ...)                                                                                   \
    enum Enum : uint8_t { Unknown = 0, __VA_ARGS__ } value = Unknown;                                           \
    NAME ( Enum value ) : value ( value ) {}                                                                    \
    std::string str() const override {                                                                          \
        static const std::vector<std::string> list = split ( "Unknown, " #__VA_ARGS__, ", " );                  \
        return #NAME "::" + list[value];                                                                        \
    }                                                                                                           \
    bool operator== ( const NAME& other ) const { return value == other.value; }                                \
    bool operator!= ( const NAME& other ) const { return value != other.value; }                                \
    bool operator== ( Enum other ) const { return value == other; }                                             \
    bool operator!= ( Enum other ) const { return value != other; }


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

// String to lower/upper case
inline std::string toLower ( std::string str ) { for ( char& c : str ) c = std::tolower ( c ); return str; }
inline std::string toUpper ( std::string str ) { for ( char& c : str ) c = std::toupper ( c ); return str; }


// String formatting functions
template<typename T>
inline std::string toString ( const T& val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

template<>
inline std::string toString<std::string> ( const std::string& val )
{
    size_t i = val.find ( "%%" );

    if ( i == std::string::npos )
        return val;

    return val.substr ( 0, i ) + "%" + toString ( val.substr ( i + 2 ) );
}

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


// Exception types
struct Exception
{
    std::string msg;

    Exception() {}
    Exception ( const std::string& msg ) : msg ( msg ) {}

    virtual std::string str() const;
};

struct WindowsException : public Exception
{
    int code = 0;

    WindowsException() {}
    WindowsException ( int code );

    std::string str() const;
};

std::ostream& operator<< ( std::ostream& os, const Exception& exception );


// Find the first window handle with the given title (NOT thread safe)
void *enumFindWindow ( const std::string& title );

// Detect if we're running on Wine
bool detectWine();

// Write to a memory location in the same process, returns 0 on success
int memwrite ( void *dst, const void *src, size_t len );


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
    std::map<std::string, std::string> settings;
    std::map<std::string, Type> types;

public:
    std::string getString ( const std::string& key ) const;
    void putString ( const std::string& key, const std::string& str );

    int getInteger ( const std::string& key ) const;
    void putInteger ( const std::string& key, int i );

    bool save ( const std::string& file ) const;
    bool load ( const std::string& file );
};


template<typename T>
inline void deleteArray ( T *ptr ) { delete[] ptr; }


// Return a sorted list with increasing order
template<typename T>
inline T sorted ( const T& list )
{
    std::vector<typename T::const_pointer> ptrs;
    ptrs.reserve ( list.size() );
    for ( const auto& x : list )
        ptrs.push_back ( &x );

    std::sort ( ptrs.begin(), ptrs.end(),
    [] ( typename T::const_pointer a, typename T::const_pointer b ) { return ( *a ) < ( *b ); } );

    T sorted;
    sorted.reserve ( ptrs.size() );
    for ( const auto& x : ptrs )
        sorted.push_back ( *x );
    return sorted;
}

// Return a sorted list with provided comparison function
template<typename T, typename F>
inline T sorted ( const T& list, const F& compare )
{
    std::vector<typename T::const_pointer> ptrs;
    ptrs.reserve ( list.size() );
    for ( const auto& x : list )
        ptrs.push_back ( &x );

    std::sort ( ptrs.begin(), ptrs.end(),
    [&] ( typename T::const_pointer a, typename T::const_pointer b ) { return compare ( *a, *b ); } );

    T sorted;
    sorted.reserve ( ptrs.size() );
    for ( const auto& x : ptrs )
        sorted.push_back ( *x );
    return sorted;
}


// Clipboard manipulation
std::string getClipboard();
void setClipboard ( const std::string& str );


// True if x is a power of two
inline bool isPowerOfTwo ( uint32_t x )
{
    return ( x != 0 ) && ( ( x & ( x - 1 ) ) == 0 );
}
