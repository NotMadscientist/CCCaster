#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cctype>
#include <type_traits>
#include <algorithm>


#define PRINT(...) do { std::cout << format ( __VA_ARGS__ ) << std::endl; } while ( 0 )


// Convert a boolean value to a type
template<bool> struct bool2type {};

// Format bytes as a hex string
std::string formatAsHex ( const std::string& bytes );
std::string formatAsHex ( const void *bytes, size_t len );

// String trim
std::string trimmed ( std::string str, const std::string& ws = " \t\r\n" );

// String split
std::vector<std::string> split ( const std::string& str, const std::string& delim = " " );

// String to lower / upper case
inline std::string lowerCase ( std::string str ) { for ( char& c : str ) c = std::tolower ( c ); return str; }
inline std::string upperCase ( std::string str ) { for ( char& c : str ) c = std::toupper ( c ); return str; }


// Format a single printable value
template<typename T>
inline std::string format ( const T& val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

// Format a string, correctly escaping any % characters
template<>
inline std::string format<std::string> ( const std::string& val )
{
    size_t i = val.find ( "%%" );

    if ( i == std::string::npos )
        return val;

    return val.substr ( 0, i ) + "%" + format ( val.substr ( i + 2 ) );
}

// For non-integer types
template<typename T>
inline void printToString ( char *buffer, size_t len, const char *fmt, const T& val, bool2type<false> )
{
    std::snprintf ( buffer, len, fmt, format ( val ).c_str() );
}

// For integer types
template<typename T>
inline void printToString ( char *buffer, size_t len, const char *fmt, const T& val, bool2type<true> )
{
    std::snprintf ( buffer, len, fmt, val );
}

// Split a format string into first parameter and rest of string
void splitFormat ( const std::string& fmt, std::string& first, std::string& rest );

// Format a string with arguments
template<typename T, typename ... V>
inline std::string format ( const std::string& fmt, const T& val, V ... vals )
{
    std::string first, rest;
    splitFormat ( fmt, first, rest );

    if ( first.empty() )
        return rest;

    char buffer[4096];
    printToString ( buffer, sizeof ( buffer ), first.c_str(), val,
                    bool2type < std::is_arithmetic<T>::value || std::is_pointer<T>::value > () );

    if ( rest.empty() )
        return buffer;

    return buffer + format ( rest, vals... );
}


// Parse a hex string
template <typename T>
inline T parseHex ( const std::string& str )
{
    T val;
    std::stringstream ss ( str );
    ss >> std::hex >> val;
    return val;
}


// Lexical cast
template <typename T>
inline T lexical_cast ( const std::string& str, T fallback = 0 )
{
    T val;
    std::stringstream ss ( str );
    ss >> val;
    if ( ss.fail() )
        return fallback;
    return val;
}


// Normalize a path to Windows backslash format and make sure there is a trailing backslash
inline std::string normalizeWindowsPath ( std::string path )
{
    path = path.substr ( 0, path.find_last_of ( "/\\" ) );

    std::replace ( path.begin(), path.end(), '/', '\\' );

    if ( !path.empty() && path.back() != '\\' )
        path += '\\';

    return path;
}
