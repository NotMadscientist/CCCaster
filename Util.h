#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <type_traits>
#include <cstdio>

#define TO_C_STR(...) toString ( __VA_ARGS__ ).c_str()

#define LOG_LIST(LIST, TO_STRING)                                       \
    do {                                                                \
        if ( !Log::isEnabled )                                          \
            break;                                                      \
        string list;                                                    \
        for ( const auto& val : LIST )                                  \
            list += " " + TO_STRING ( val ) + ",";                      \
        if ( !LIST.empty() )                                            \
            list [ list.size() - 1 ] = ' ';                             \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list );                 \
    } while ( 0 )

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
void printToString ( char *buffer, const char *format, const T& val, bool2type<false> )
{
    std::sprintf ( buffer, format, TO_C_STR ( val ) );
}

template<typename T>
void printToString ( char *buffer, const char *format, const T& val, bool2type<true> )
{
    std::sprintf ( buffer, format, val );
}

template<typename T, typename ... V>
inline std::string toString ( const std::string& format, const T& val, V ... vals )
{
    std::string first, rest;
    splitFormat ( format, first, rest );

    if ( first.empty() )
        return rest;

    char buffer[4096];
    printToString ( buffer, first.c_str(), val, bool2type<std::is_arithmetic<T>::value>() );

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

inline std::string toBase64 ( const char *bytes, size_t len )
{
    if ( len == 0 )
        return "";

    std::string str;
    for ( size_t i = 0; i < len; ++i )
        str += toString ( "%02x ", ( unsigned char ) bytes[i] );

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

// Convert the Windows error to a formatted string
std::string getWindowsErrorAsString ( int error );

// Get the last Windows API error as a formatted string
std::string getLastWindowsError();

// Get the last Windows socket error as a formatted string
std::string getLastWinSockError();
