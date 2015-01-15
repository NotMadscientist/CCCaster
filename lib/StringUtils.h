#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cctype>
#include <type_traits>


#define PRINT(...) do { std::cout << format ( __VA_ARGS__ ) << std::endl; } while ( 0 )


// Convert a boolean value to a type
template<bool> struct bool2type {};

// Split a format string into first parameter and rest of string
void splitFormat ( const std::string& fmt, std::string& first, std::string& rest );

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


// String formatting functions
template<typename T>
inline std::string format ( const T& val )
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

template<>
inline std::string format<std::string> ( const std::string& val )
{
    size_t i = val.find ( "%%" );

    if ( i == std::string::npos )
        return val;

    return val.substr ( 0, i ) + "%" + format ( val.substr ( i + 2 ) );
}

template<typename T>
void printToString ( char *buffer, size_t len, const char *fmt, const T& val, bool2type<false> ) // non-integer types
{
    std::snprintf ( buffer, len, fmt, format ( val ).c_str() );
}

template<typename T>
void printToString ( char *buffer, size_t len, const char *fmt, const T& val, bool2type<true> ) // integer types
{
    std::snprintf ( buffer, len, fmt, val );
}

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

