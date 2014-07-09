#include "Util.h"
#include "Log.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#include <md5.h>
#include <winsock2.h>

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

    LOG ( "zlib error: [%d] %s", rc, mz_error ( rc ) );
    return 0;
}

size_t uncompress ( const char *src, size_t srcLen, char *dst, size_t dstLen )
{
    mz_ulong len = dstLen;
    int rc = mz_uncompress ( ( unsigned char * ) dst, &len, ( unsigned char * ) src, srcLen );
    if ( rc == MZ_OK )
        return len;

    LOG ( "zlib error: [%d] %s", rc, mz_error ( rc ) );
    return 0;
}

size_t compressBound ( size_t srcLen )
{
    return mz_compressBound ( srcLen );
}

static string getWindowsErrorAsString ( int error )
{
    string str;
    char *errorString = 0;
    FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, error, 0, ( LPSTR ) &errorString, 0, 0 );
    str = ( errorString ? trim ( errorString ) : "(null)" );
    LocalFree ( errorString );
    return str;
}

WindowsError::WindowsError ( int code ) : code ( code ), msg ( getWindowsErrorAsString ( code ) ) {}

ostream& operator<< ( ostream& os, const WindowsError& error )
{
    return ( os << "[" << error.code << "] '" << error.msg << "'" );
}

int memwrite ( void *dst, const void *src, size_t len )
{
    DWORD old, tmp;

    if ( !VirtualProtect ( dst, len, PAGE_READWRITE, &old ) )
        return GetLastError();

    memcpy ( dst, src, len );

    if ( !VirtualProtect ( dst, len, old, &tmp ) )
        return GetLastError();

    return 0;
}
