#include "Protocol.h"
#include "Protocol.types.h"

using namespace std;
using namespace cereal;

// Increase size as needed
#define MSG_TYPE_UINT uint8_t

const char *MsgType::c_str() const
{
    switch ( value )
    {
#include "Protocol.strings.h"
    }

    return "Unknown type!";
}

string Serializable::encode ( const Serializable& msg )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( ( MSG_TYPE_UINT ) msg.type().value );
    msg.serialize ( archive );

    return ss.str();
}

MsgPtr Serializable::decode ( char *bytes, size_t len )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    MSG_TYPE_UINT type;
    archive ( type );

    MsgPtr msg;

    switch ( ( MsgType::Enum ) type )
    {
#include "Protocol.decode.h"
    }

    return msg;
}
