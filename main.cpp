#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include <cstdio>
#include <cctype>
#include <sstream>
#include <string>

using namespace std;
using namespace cereal;

struct MsgVersion
{
    uint8_t major, minor;
    string sub;

    void serialize ( BinaryOutputArchive& ar )
    {
        ar ( major, minor, sub );
    }

    void deserialize ( BinaryInputArchive& ar )
    {
        ar ( major, minor, sub );
    }
};

int main ( int argc, char *argv[] )
{
    stringstream ss ( stringstream::in | stringstream::out | stringstream::binary );

    BinaryOutputArchive boa ( ss );

    MsgVersion v = { 1, 2, "asdfa" };

    v.serialize ( boa );

    BinaryInputArchive bia ( ss );

    MsgVersion u;
    u.deserialize ( bia );

    printf ( "sizeof ( size_t ) = %d\n", sizeof ( size_t ) );

    for ( char c : ss.str() )
        printf ( "%02x ", ( int ) c );
    printf ( "\n" );

    printf ( "%d %d %s\n", u.major, u.minor, u.sub.c_str() );

    return 0;
}
