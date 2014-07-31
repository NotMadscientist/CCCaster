#include "Utilities.h"
#include "Logger.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#include <md5.h>
#include <winsock2.h>
#include <windows.h>

#include <cctype>
#include <iostream>

using namespace std;


void splitFormat ( const string& format, string& first, string& rest )
{
    size_t i;

    for ( i = 0; i < format.size(); ++i )
        if ( format[i] == '%' && ( i + 1 == format.size() || format[i + 1] != '%' ) )
            break;

    if ( i == format.size() - 1 )
    {
        first = "";
        rest = format;
        return;
    }

    for ( ++i; i < format.size(); ++i )
        if ( ! ( format[i] == '.' || isalnum ( format[i] ) ) )
            break;

    first = format.substr ( 0, i );
    rest = ( i < format.size() ? format.substr ( i ) : "" );
}

string toBase64 ( const string& bytes )
{
    if ( bytes.empty() )
        return "";

    string str;
    for ( char c : bytes )
        str += toString ( "%02x ", ( unsigned char ) c );

    return str.substr ( 0, str.size() - 1 );
}

string toBase64 ( const void *bytes, size_t len )
{
    if ( len == 0 )
        return "";

    string str;
    for ( size_t i = 0; i < len; ++i )
        str += toString ( "%02x ", static_cast<const unsigned char *> ( bytes ) [i] );

    return str.substr ( 0, str.size() - 1 );
}

string trim ( string str, const string& ws )
{
    // trim trailing spaces
    size_t endpos = str.find_last_not_of ( ws );
    if ( string::npos != endpos )
        str = str.substr ( 0, endpos + 1 );

    // trim leading spaces
    size_t startpos = str.find_first_not_of ( ws );
    if ( string::npos != startpos )
        str = str.substr ( startpos );

    return str;
}

void getMD5 ( const char *bytes, size_t len, char dst[16] )
{
    MD5_CTX md5;
    MD5_Init ( &md5 );
    MD5_Update ( &md5, bytes, len );
    MD5_Final ( ( unsigned char * ) dst, &md5 );
}

void getMD5 ( const string& str, char dst[16] )
{
    getMD5 ( &str[0], str.size(), dst );
}

bool checkMD5 ( const char *bytes, size_t len, const char md5[16] )
{
    char tmp[16];
    getMD5 ( bytes, len, tmp );
    return !strncmp ( tmp, md5, sizeof ( tmp ) );
}

bool checkMD5 ( const string& str, const char md5[16] )
{
    return checkMD5 ( &str[0], str.size(), md5 );
}

size_t compress ( const char *src, size_t srcLen, char *dst, size_t dstLen, int level )
{
    mz_ulong len = dstLen;
    int rc = mz_compress2 ( ( unsigned char * ) dst, &len, ( unsigned char * ) src, srcLen, level );
    if ( rc == MZ_OK )
        return len;

    LOG ( "[%d] %s; zlib error", rc, mz_error ( rc ) );
    return 0;
}

size_t uncompress ( const char *src, size_t srcLen, char *dst, size_t dstLen )
{
    mz_ulong len = dstLen;
    int rc = mz_uncompress ( ( unsigned char * ) dst, &len, ( unsigned char * ) src, srcLen );
    if ( rc == MZ_OK )
        return len;

    LOG ( "[%d] %s; zlib error", rc, mz_error ( rc ) );
    return 0;
}

size_t compressBound ( size_t srcLen )
{
    return mz_compressBound ( srcLen );
}

static string getWindowsExceptionAsString ( int error )
{
    string str;
    char *errorString = 0;
    FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, error, 0, ( LPSTR ) &errorString, 0, 0 );
    str = ( errorString ? trim ( errorString ) : "(null)" );
    LocalFree ( errorString );
    return str;
}

WindowsException::WindowsException ( int code ) : Exception ( getWindowsExceptionAsString ( code ) ), code ( code ) {}

ostream& operator<< ( ostream& os, const Exception& error )
{
    return ( os << error.msg );
}

ostream& operator<< ( ostream& os, const WindowsException& error )
{
    return ( os << "[" << error.code << "] '" << error.msg << "'" );
}

void *enumFindWindow ( const string& title )
{
    static string tmpTitle;
    static HWND tmpHwnd;

    struct _
    {
        static BOOL CALLBACK enumWindowsProc ( HWND hwnd, LPARAM lParam )
        {
            if ( hwnd == 0 )
                return true;

            char buffer[4096];
            GetWindowText ( hwnd, buffer, sizeof ( buffer ) );

            string name = buffer;
            while ( !name.empty() && name.back() == ' ' )
                name.pop_back();
            while ( !name.empty() && name[0] == ' ' )
                name = name.substr ( 1 );

            if ( tmpTitle == name )
                tmpHwnd = hwnd;
            return true;
        }
    };

    tmpTitle = title;
    tmpHwnd = 0;
    EnumWindows ( _::enumWindowsProc, 0 );
    return tmpHwnd;
}

bool detectWine()
{
    static char isWine = -1;

    if ( isWine >= 0 )
        return isWine;

    HMODULE hntdll = GetModuleHandle ( "ntdll.dll" );

    if ( !hntdll )
    {
        isWine = 0;
        return isWine;
    }

    isWine = ( GetProcAddress ( hntdll, "wine_get_version" ) ? 1 : 0 );
    return isWine;
}
