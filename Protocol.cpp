#include "Protocol.h"
#include "Protocol.types.h"

using namespace std;
using namespace cereal;

ostream& operator<< ( ostream& os, const MsgType& type )
{
    switch ( type )
    {
#include "Protocol.strings.h"
    }

    return ( os << "Unknown type!" );
}

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
