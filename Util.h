#pragma once

#include <vector>
#include <string>
#include <sstream>
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

// String utilities
template<typename T>
inline std::string toString ( const T& val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

template<>
inline std::string toString<std::string> ( const std::string& val ) { return val; }

inline void accumulateToString ( std::vector<std::string>& args ) { return; }

template<typename T, typename ... V>
inline void accumulateToString ( std::vector<std::string>& args, const T& val, V ... vals )
{
    args.push_back ( toString ( val ) );
    accumulateToString ( args, vals... );
}

template<typename T, typename ... V>
inline std::string toString ( const char *format, const T& val, V ... vals )
{
    std::vector<std::string> args;
    accumulateToString ( args, val, vals... );

    char buffer[4096];

    switch ( args.size() )
    {
#include "Util.string.h"
    }

    return buffer;
}

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
