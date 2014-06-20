#include "Protocol.h"
#include "Protocol.types.h"

#include <cassert>

using namespace std;
using namespace cereal;

string Serializable::encode ( Serializable *message )
{
    if ( !message )
        return "";

    MsgPtr msg ( message );
    return encode ( msg );
}

string Serializable::encode ( const MsgPtr& msg )
{
    if ( !msg.get() )
        return "";

    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( msg->type() );
    msg->serializeBase ( archive );
    msg->serialize ( archive );

    return ss.str();
}

MsgPtr Serializable::decode ( char *bytes, size_t len, size_t& consumed )
{
    MsgPtr msg;

    if ( len == 0 )
        return msg;

    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    MsgType type;
    archive ( type );

    switch ( type )
    {
#include "Protocol.decode.h"
    }

    string remaining;
    getline ( ss, remaining );

    assert ( len >= remaining.size() );
    consumed = len - remaining.size();

    return msg;
}

ostream& operator<< ( ostream& os, const MsgPtr& msg )
{
    if ( !msg.get() )
        return ( os << "NullMsg" );
    else
        return ( os << msg->type() );
}

ostream& operator<< ( ostream& os, MsgType type )
{
    switch ( type )
    {
#include "Protocol.strings.h"
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
    }

    return ( os << "Unknown type!" );
}
