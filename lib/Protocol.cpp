#include "Protocol.h"
#include "Protocol.types.h"
#include "Utilities.h"
#include "Logger.h"

#include <cassert>

using namespace std;
using namespace cereal;

// Useful options for debugging and testing
// #define LOG_DECODE
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
enum DecodeResult { Failed = 0, NotCompressed, Compressed };

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

    // Encode msg data
    msg->saveBase ( archive );
    msg->save ( archive );

    // Update MD5
    if ( msg->md5empty )
    {
        getMD5 ( ss.str(), msg->md5 );
        msg->md5empty = false;
    }

    // Encode MD5 at end of msg data
    archive ( msg->md5 );

    // Compress
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
    DecodeResult result;

    // Decompress
    if ( ! ( result = decodeStageTwo ( bytes, len, consumed, type, data ) ) )
    {
#ifdef LOG_DECODE
        LOG ( "decodeStageTwo: result=Failed" );
#endif
        consumed = 0;
        return NullMsg;
    }

#ifdef LOG_DECODE
    LOG ( "decodeStageTwo: result=%s", ( result == Compressed ) ? "Compressed" : "NotCompressed" );
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

        // Decode msg data
        msg->loadBase ( archive );
        msg->load ( archive );

        // Decode MD5 at end of msg data
        archive ( msg->md5 );
        msg->md5empty = false;
    }
    catch ( ... )
    {
#ifdef LOG_DECODE
        // TODO log the exception
        LOG ( "invalid binary format for type=%s", type );
#endif
        msg.reset();
    }

    if ( !msg.get() )
    {
        consumed = 0;
        return NullMsg;
    }

    // decodeStageTwo does not update the value of consumed if the data was not compressed
    if ( result == NotCompressed )
    {
        // Check for unread bytes
        string remaining;
        getline ( ss, remaining );

        assert ( len >= remaining.size() );
        consumed = ( len - remaining.size() );
    }

    if ( !checkMD5 ( &data[0], data.size() - sizeof ( msg->md5 ), msg->md5 ) )
    {
#ifdef LOG_DECODE
        LOG ( "MD5 checksum failed" );
#endif
        return NullMsg;
    }

    return msg;
}

string encodeStageTwo ( const MsgPtr& msg, const string& msgData )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    // Encode msg type first without compression
    archive ( msg->getMsgType() );

    // Compress msg data if needed
    if ( msg->compressionLevel )
    {
        string buffer ( compressBound ( msgData.size() ), ( char ) 0 );
        size_t size = compress ( &msgData[0], msgData.size(), &buffer[0], buffer.size(), msg->compressionLevel );
        buffer.resize ( size );

        // Only use compressed msg data if actually smaller after the overhead
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
        // Decode msg type first before decompression
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
        return Failed;
    }

    // Get remaining bytes
    string remaining;
    getline ( ss, remaining );

    // Decompress msg data if needed
    if ( compressionLevel )
    {
        string buffer ( uncompressedSize, ( char ) 0 );
        size_t size = uncompress ( &msgData[0], msgData.size(), &buffer[0], buffer.size() );

        if ( size != uncompressedSize )
        {
            consumed = 0;
            return Failed;
        }

        // Check for unread bytes
        assert ( len >= remaining.size() );
        consumed = len - remaining.size();

        msgData = buffer;
        return Compressed;
    }

    msgData = remaining;
    return NotCompressed;
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

ostream& operator<< ( ostream& os, BaseType type )
{
    switch ( type )
    {
        case BaseType::SerializableMessage:
            return ( os << "SerializableMessage" );

        case BaseType::SerializableSequence:
            return ( os << "SerializableSequence" );

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
