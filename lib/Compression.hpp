#pragma once

#include <string>


// MD5 calculation
void getMD5 ( const char *bytes, size_t len, char dst[16] );
void getMD5 ( const std::string& str, char dst[16] );
bool checkMD5 ( const char *bytes, size_t len, const char md5[16] );
bool checkMD5 ( const std::string& str, const char md5[16] );


// zlib compression
size_t compress ( const char *src, size_t srcLen, char *dst, size_t dstLen, int level = 9 );
size_t uncompress ( const char *src, size_t srcLen, char *dst, size_t dstLen );
size_t compressBound ( size_t srcLen );
