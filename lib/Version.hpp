#pragma once

#include "Protocol.hpp"

#include <cereal/types/string.hpp>

#include <string>


class Version : public SerializableSequence
{
public:

    enum PartEnum { Major, Minor, Suffix };

    std::string code;
    std::string revision;
    std::string buildTime;

    Version ( const char *code ) : code ( code ) {}

    Version ( const std::string& code ) : code ( code ) {}

    Version ( const std::string& code, const std::string& revision, const std::string& buildTime )
        : code ( code ), revision ( revision ), buildTime ( buildTime ) {}

    std::string majorMinor() const { return major() + "." + minor(); }

    std::string major() const { return get ( Major ); }

    std::string minor() const { return get ( Minor ); }

    std::string suffix() const { return get ( Suffix ); }

    bool isCustom() const;

#ifdef RELEASE
    bool isSimilar ( const Version& other, uint8_t level = 1 ) const;
#else
    bool isSimilar ( const Version& other, uint8_t level = 0xFF ) const;
#endif

    void clear()
    {
        code.clear();
        revision.clear();
        buildTime.clear();
    }

    bool empty() const
    {
        return code.empty();
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( Version, code, revision, buildTime )

private:

    std::string get ( PartEnum part ) const;
};


// Comparison operators
bool operator< ( const Version& a, const Version& b );
inline bool operator<= ( const Version& a, const Version& b ) { return ( a < b ) || a.isSimilar ( b, 2 ); }
inline bool operator> ( const Version& a, const Version& b ) { return ! ( a <= b ); }
inline bool operator>= ( const Version& a, const Version& b ) { return ! ( a < b ); }


// Stream operator
inline std::ostream& operator<< ( std::ostream& os, const Version& a ) { return ( os << a.code ); }


extern const Version LocalVersion;
