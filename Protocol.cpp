#include "Protocol.h"
#include "Protocol.type.h"

// Increase the size of this type as needed
typedef uint8_t SerializableTypeUInt;

using namespace std;
using namespace cereal;

string Serializable::encode ( const Serializable& msg )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( ( SerializableTypeUInt ) msg.type() );
    msg.serialize ( archive );

    return ss.str();
}

MsgPtr Serializable::decode ( char *bytes, size_t len )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    SerializableTypeUInt type;
    archive ( type );

    MsgPtr msg;

    switch ( ( SerializableType ) type )
    {
#include "Protocol.decode.h"
    }

    return msg;
}
