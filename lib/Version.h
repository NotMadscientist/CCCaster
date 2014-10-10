#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>


struct Version : public SerializableSequence
{
    enum LocalEnum { Local };

    enum PartEnum { Major, Minor, Suffix };

    std::string code;
    std::string commitId;
    std::string buildTime;

    Version ( LocalEnum );

    std::string major() const { return get ( Major ); }

    std::string minor() const { return get ( Minor ); }

    std::string suffix() const { return get ( Suffix ); }

    bool compare ( const Version& other, uint8_t level = 0 ) const;

    PROTOCOL_MESSAGE_BOILERPLATE ( Version, code, commitId, buildTime )

private:

    std::string get ( PartEnum part ) const;
};


const Version LocalVersion ( Version::Local );
