#include "Util.h"
#include "Log.h"

#include <md5.h>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

using namespace std;

string getMD5 ( const char *bytes, size_t len )
{
    unsigned char md5sum[17];
    md5sum[16] = '\0';

    MD5_CTX md5;
    MD5_Init ( &md5 );
    MD5_Update ( &md5, bytes, len );
    MD5_Final ( md5sum, &md5 );

    return string ( ( char * ) md5sum );
}

string getMD5 ( const string& str )
{
    return getMD5 ( &str[0], str.size() );
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
