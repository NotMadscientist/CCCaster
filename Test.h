#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>

struct TestMessage : public SerializableSequence
{
    std::string str;

    TestMessage() {}

    TestMessage ( const std::string& str ) : str ( str ) {}

    MsgType type() const;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( str ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( str ); }
};

int RunAllTests ( int& argc, char *argv[] );
