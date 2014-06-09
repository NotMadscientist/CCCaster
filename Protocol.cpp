#include "Protocol.h"
#include "Protocol.type.h"

using namespace std;
using namespace cereal;

string Serializable::encode ( const Serializable& msg )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( ( uint8_t ) msg.type() );
    msg.serialize ( archive );

    return ss.str();
}

MsgPtr Serializable::decode ( char *bytes, size_t len )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    uint8_t type;
    archive ( type );

    MsgPtr msg;

    switch ( ( SerializableType ) type )
    {
#include "Protocol.decode.h"
    }

    return msg;
}
