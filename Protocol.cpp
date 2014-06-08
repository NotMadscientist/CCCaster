#include "Protocol.h"
#include "IpAddrPort.h"

using namespace std;
using namespace cereal;

string Serializable::encode ( const Serializable& msg )
{
    ostringstream ss ( stringstream::binary );
    BinaryOutputArchive archive ( ss );

    archive ( msg.type() );
    msg.serialize ( archive );

    return ss.str();
}

shared_ptr<Serializable> Serializable::decode ( char *bytes, size_t len )
{
    istringstream ss ( string ( bytes, len ), stringstream::binary );
    BinaryInputArchive archive ( ss );

    uint32_t type;
    archive ( type );

    shared_ptr<Serializable> msg;

    if ( type == 1 )
    {
        msg.reset ( new IpAddrPort() );
        msg->deserialize ( archive );
    }

    return msg;
}

uint32_t IpAddrPort::type() const { return 1; }
