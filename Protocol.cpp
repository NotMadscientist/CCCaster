#include "Protocol.h"
#include "Protocol.types.h"

using namespace std;
using namespace cereal;

string Serializable::encode ( const Serializable& msg )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( msg.type() );

    switch ( msg.base() )
    {
        case BaseType::SerializableMessage:
            break;

        case BaseType::SerializableSequence:
            archive ( static_cast<const SerializableSequence&> ( msg ).sequence );
            break;
    }

    msg.serialize ( archive );

    return ss.str();
}

MsgPtr Serializable::decode ( char *bytes, size_t len )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    MsgType type;
    archive ( type );

    MsgPtr msg;

    switch ( type )
    {
#include "Protocol.decode.h"
    }

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
