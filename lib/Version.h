#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>


struct Version : public SerializableSequence
{
    enum LocalEnum { Local };

    std::string commitId;
    std::string buildTime;
    std::string code;

    Version ( LocalEnum );

    PROTOCOL_MESSAGE_BOILERPLATE ( Version, commitId, buildTime, code )
};


const Version LocalVersion ( Version::Local );
