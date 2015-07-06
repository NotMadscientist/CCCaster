#include "Compression.hpp"
#include "Logger.hpp"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#include <md5.h>

#include <cstring>

using namespace std;


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
