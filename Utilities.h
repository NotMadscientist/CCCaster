#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <type_traits>
#include <cstdio>

#define TO_C_STR(...) toString ( __VA_ARGS__ ).c_str()

#define INLINE_DWORD(X)                                                         \
    static_cast<unsigned char> ( unsigned ( X ) & 0xFF ),                       \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 8 ) & 0xFF ),              \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 16 ) & 0xFF ),             \
    static_cast<unsigned char> ( ( unsigned ( X ) >> 24 ) & 0xFF )

// Convert a boolean value to a type
template<bool> struct bool2type {};

// Split a format string into first parameter and rest of string
void splitFormat ( const std::string& format, std::string& first, std::string& rest );

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

// Format bytes as a base 64 string
inline std::string toBase64 ( const std::string& bytes )
{
    if ( bytes.empty() )
        return "";

    std::string str;
    for ( char c : bytes )
        str += toString ( "%02x ", ( unsigned char ) c );

    return str.substr ( 0, str.size() - 1 );
}

inline std::string toBase64 ( const void *bytes, size_t len )
{
    if ( len == 0 )
        return "";

    std::string str;
    for ( size_t i = 0; i < len; ++i )
        str += toString ( "%02x ", static_cast<const unsigned char *> ( bytes ) [i] );

    return str.substr ( 0, str.size() - 1 );
}

// String trim
inline std::string trim ( std::string str, const std::string& ws = " \t\r\n" )
{
    // trim trailing spaces
    size_t endpos = str.find_last_not_of ( ws );
    if ( std::string::npos != endpos )
        str = str.substr ( 0, endpos + 1 );

    // trim leading spaces
    size_t startpos = str.find_first_not_of ( ws );
    if ( std::string::npos != startpos )
        str = str.substr ( startpos );

    return str;
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

    inline Exception() {}
    inline Exception ( const std::string& msg ) : msg ( msg ) {}
};

// Windows exception type
struct WindowsException : public Exception
{
    int code;

    inline WindowsException() : code ( 0 ) {}
    WindowsException ( int code );
};

std::ostream& operator<< ( std::ostream& os, const Exception& exception );
std::ostream& operator<< ( std::ostream& os, const WindowsException& error );

#define LOG_AND_THROW(EXCEPTION, FORMAT, ...)                               \
    do {                                                                    \
        LOG ( "%s; " FORMAT, EXCEPTION, ## __VA_ARGS__ );                   \
        throw EXCEPTION;                                                    \
    } while ( 0 )

// Write to a memory location in the same process, returns 0 on success
int memwrite ( void *dst, const void *src, size_t len );

// Struct for storing assembly code
struct Asm
{
    void *const addr;
    const std::vector<uint8_t> bytes;

    WindowsException write() const { return WindowsException ( memwrite ( addr, &bytes[0], bytes.size() ) ); }
};

// Hash util
namespace std
{

template<class T>
inline void hash_combine ( size_t& seed, const T& v )
{
    hash<T> hasher;
    seed ^= hasher ( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

} // namespace std
