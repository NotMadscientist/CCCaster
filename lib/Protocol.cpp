#include "Protocol.h"
#include "Protocol.types.h"
#include "Utilities.h"
#include "Logger.h"

#include <cassert>

using namespace std;
using namespace cereal;

// Useful options for debugging and testing
// #define LOG_PROTOCOL
// #define FORCE_COMPRESSION


/* Message binary structure:

Compressed:

    1 byte  message type
    1 byte  compression level
    4 byte  uncompressed size
    4 byte  compressed data size
    ...     compressed data
            ========================
            ...     raw data
            16 byte MD5 checksum
            ========================

Not compressed:

    1 byte  message type
    1 byte  compression level
    ========================
    ...     raw data
    16 byte MD5 checksum
    ========================

*/


// Basic constructor with default compression level
Serializable::Serializable() : compressionLevel ( 9 ) {}


// Encode with compression
string encodeStageTwo ( const MsgPtr& msg, const string& msgData );

// Result of the decode
ENUM ( DecodeResult, Failed, NotCompressed, Compressed );

// Decode with compression. Must manually update the value of consumed if the data was not compressed.
DecodeResult decodeStageTwo ( const char *bytes, size_t len, size_t& consumed, MsgType& type, string& msgData );

string Protocol::encode ( Serializable *message )
{
    if ( !message )
        return "";

    MsgPtr msg ( message );
    return encode ( msg );
}

string Protocol::encode ( const MsgPtr& msg )
{
    if ( !msg.get() )
        return "";

    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    // Encode base message data
    msg->saveBase ( archive );

    // Encode actual message data
    msg->save ( archive );

    // Update MD5
    if ( msg->md5empty )
    {
        getMD5 ( ss.str(), msg->md5 );
        msg->md5empty = false;

#ifdef LOG_PROTOCOL
        LOG ( "%s", msg->getMsgType() );
        if ( ss.str().size() <= 256 )
            LOG ( "data=[ %s ]", toBase64 ( ss.str() ) );
        LOG ( "md5=[ %s ]", toBase64 ( msg->md5, sizeof ( msg->md5 ) ) );
#endif
    }

    // Encode MD5 at end of message data
    archive ( msg->md5 );

    // Encode with compression
    return encodeStageTwo ( msg, ss.str() );
}

MsgPtr Protocol::decode ( const char *bytes, size_t len, size_t& consumed )
{
    MsgPtr msg;

    if ( len == 0 )
    {
        consumed = 0;
        return NullMsg;
    }

    MsgType type;
    string data;

    // Decode with compression
    DecodeResult result = decodeStageTwo ( bytes, len, consumed, type, data );

#ifdef LOG_PROTOCOL
    LOG ( "decodeStageTwo: result=%s", result );
#endif

    if ( result == DecodeResult::Failed )
    {
        consumed = 0;
        return NullMsg;
    }

#ifdef LOG_PROTOCOL
    if ( data.size() <= 256 )
        LOG ( "decodeStageTwo: data=[ %s ]", toBase64 ( data ) );
#endif

    istringstream ss ( data, stringstream::binary );
    BinaryInputArchive archive ( ss );

    try
    {
        // Construct the correct message type
        switch ( type )
        {
#include "Protocol.decode.h"

            default:
                consumed = 0;
                return NullMsg;
        }

        // Decode base message data
        msg->loadBase ( archive );

        // Decode actual message data
        msg->load ( archive );

        // Decode MD5 at end of message data
        archive ( msg->md5 );
        msg->md5empty = false;
    }
    catch ( const cereal::Exception& err )
    {
#ifdef LOG_PROTOCOL
        LOG ( "type=%s; cereal::Exception: '%s'", type, err.what() );
#endif
        msg.reset();
    }

    if ( !msg.get() )
    {
        consumed = 0;
        return NullMsg;
    }

    // decodeStageTwo does not update the value of consumed if the data was not compressed
    if ( result == DecodeResult::NotCompressed )
    {
        // Check for unread bytes
        size_t remaining = ss.rdbuf()->in_avail();
        assert ( len >= remaining );
        consumed = ( len - remaining );
    }

//     if ( !checkMD5 ( &data[0], consumed - sizeof ( msg->md5 ), msg->md5 ) )
//     {
// #ifdef LOG_PROTOCOL
//         LOG ( "MD5 checksum failed for %s", type );
//         LOG ( "data=[ %s ]", toBase64 ( &data[0], consumed - sizeof ( msg->md5 ) ) );
//         LOG ( "md5     =[ %s ]", toBase64 ( msg->md5, sizeof ( msg->md5 ) ) );
//         char md5[sizeof ( msg->md5 )];
//         getMD5 ( &data[0], consumed - sizeof ( msg->md5 ), md5 );
//         LOG ( "expected=[ %s ]", toBase64 ( md5, sizeof ( msg->md5 ) ) );
// #endif
//         return NullMsg;
//     }

    return msg;
}

string encodeStageTwo ( const MsgPtr& msg, const string& msgData )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    // Encode message type first without compression
    archive ( msg->getMsgType() );

    // Compress message data if needed
    if ( msg->compressionLevel )
    {
        string buffer ( compressBound ( msgData.size() ), ( char ) 0 );
        size_t size = compress ( &msgData[0], msgData.size(), &buffer[0], buffer.size(), msg->compressionLevel );
        buffer.resize ( size );

        // Only use compressed message data if actually smaller after the overhead
#ifndef FORCE_COMPRESSION
        if ( sizeof ( msgData.size() ) + sizeof ( buffer.size() ) + buffer.size() < msgData.size() )
#endif
        {
            archive ( msg->compressionLevel );
            archive ( msgData.size() );         // uncompressed size
            archive ( buffer );                 // compressed size + compressed data
            return ss.str();
        }

        // Otherwise update compression level so we don't try to compress this again
        msg->compressionLevel = 0;
    }

    // uncompressed data does not include uncompressedSize or any other sizes
    archive ( msg->compressionLevel );
    return ss.str() + msgData;
}

DecodeResult decodeStageTwo ( const char *bytes, size_t len, size_t& consumed, MsgType& type, string& msgData )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    uint8_t compressionLevel;
    uint32_t uncompressedSize;

    try
    {
        // Decode message type first before decompression
        archive ( type );
        archive ( compressionLevel );

        // Only compressed data includes uncompressedSize + a compressed data buffer
        if ( compressionLevel )
        {
            archive ( uncompressedSize );       // uncompressed size
            archive ( msgData );                // compressed size + compressed data
        }
    }
    catch ( ... )
    {
        consumed = 0;
        return DecodeResult::Failed;
    }

    // Get remaining bytes
    size_t remaining = ss.rdbuf()->in_avail();
    assert ( len >= remaining );

    // Decompress message data if needed
    if ( compressionLevel )
    {
        string buffer ( uncompressedSize, ( char ) 0 );
        size_t size = uncompress ( &msgData[0], msgData.size(), &buffer[0], buffer.size() );

        if ( size != uncompressedSize )
        {
            consumed = 0;
            return DecodeResult::Failed;
        }

        // Update consumed bytes
        consumed = len - remaining;
        msgData = buffer;
        return DecodeResult::Compressed;
    }

    // Get remaining bytes
    msgData.resize ( remaining );
    ss.rdbuf()->sgetn ( &msgData[0], remaining );
    return DecodeResult::NotCompressed;
}

ostream& operator<< ( ostream& os, MsgType type )
{
    switch ( type )
    {
#include "Protocol.strings.h"

        default:
            break;
    }

    return ( os << "Unknown type!" );
}

ostream& operator<< ( ostream& os, const MsgPtr& msg )
{
    if ( !msg.get() )
        return ( os << "NullMsg" );
    else
        return ( os << msg->str() );
}

ostream& operator<< ( ostream& os, const Serializable& msg )
{
    return ( os << msg.str() );
}
