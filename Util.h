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

// String trim
inline std::string trim ( std::string str, const std::string& ws = " \t\r\n" )
{
    // trim trailing spaces
    std::size_t endpos = str.find_last_not_of ( ws );
    if ( std::string::npos != endpos )
        str = str.substr ( 0, endpos + 1 );

    // trim leading spaces
    std::size_t startpos = str.find_first_not_of ( ws );
    if ( std::string::npos != startpos )
        str = str.substr ( startpos );

    return str;
}

// MD5 calculation
std::string getMD5 ( const char *bytes, std::size_t len );
std::string getMD5 ( const std::string& str );

// zlib compression
std::size_t compress ( const char *src, std::size_t srcLen, char *dst, std::size_t dstLen, int level = 9 );
std::size_t uncompess ( const char *src, std::size_t srcLen, char *dst, std::size_t dstLen );
std::size_t compressBound ( std::size_t srcLen );
