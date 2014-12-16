#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>


struct Version : public SerializableSequence
{
    enum PartEnum { Major, Minor, Suffix };

    std::string code;
    std::string revision;
    std::string buildTime;

    Version ( const char *code ) : code ( code ) {}

    Version ( const std::string& code ) : code ( code ) {}

    Version ( const std::string& code, const std::string& revision, const std::string& buildTime )
        : code ( code ), revision ( revision ), buildTime ( buildTime ) {}

    std::string major() const { return get ( Major ); }

    std::string minor() const { return get ( Minor ); }

    std::string suffix() const { return get ( Suffix ); }

    bool isCustom() const;

#ifdef RELEASE
    bool similar ( const Version& other, uint8_t level = 1 ) const;
#else
    bool similar ( const Version& other, uint8_t level = 0xFF ) const;
#endif

    void clear()
    {
        code.clear();
        revision.clear();
        buildTime.clear();
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( Version, code, revision, buildTime )

private:

    std::string get ( PartEnum part ) const;
};


// Comparison operator
bool operator< ( const Version& a, const Version& b );
inline bool operator<= ( const Version& a, const Version& b ) { return ( a < b ) || a.similar ( b, 2 ); }
inline bool operator> ( const Version& a, const Version& b ) { return ! ( a <= b ); }
inline bool operator>= ( const Version& a, const Version& b ) { return ! ( a < b ); }


// Stream operator
inline std::ostream& operator<< ( std::ostream& os, const Version& a ) { return ( os << a.code ); }


extern const Version LocalVersion;
