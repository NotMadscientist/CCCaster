#pragma once

#include <string>
#include <sstream>
#include <cstdio>

// Lexical cast
template<typename T> std::string toString ( T val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}
template<typename T> std::string toString ( const char *fmt, T val )
{
    char buffer[4096];
    std::sprintf ( buffer, fmt, val );
    return buffer;
}

#define TO_C_STR(...) toString ( __VA_ARGS__ ).c_str()

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
std::string getMD5 ( const char *bytes, size_t len );
std::string getMD5 ( const std::string& str );

// zlib compression
size_t compress ( const char *src, size_t srcLen, char *dst, size_t dstLen, int level = 9 );
size_t uncompess ( const char *src, size_t srcLen, char *dst, size_t dstLen );
size_t compressBound ( size_t srcLen );
