#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>


struct Version : public SerializableSequence
{
    enum PartEnum { Major, Minor, Suffix };

    std::string code;
    std::string commitId;
    std::string buildTime;

    Version ( const std::string& code, const std::string& commitId, const std::string& buildTime )
        : code ( code ), commitId ( commitId ), buildTime ( buildTime ) {}

    std::string major() const { return get ( Major ); }

    std::string minor() const { return get ( Minor ); }

    std::string suffix() const { return get ( Suffix ); }

    bool similar ( const Version& other, uint8_t level = 0xFF ) const;

    PROTOCOL_MESSAGE_BOILERPLATE ( Version, code, commitId, buildTime )

private:

    std::string get ( PartEnum part ) const;
};


// Stream operator
inline std::ostream& operator<< ( std::ostream& os, const Version& a ) { return ( os << a.code ); }


extern const Version LocalVersion;
