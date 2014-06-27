#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <string>

#define TEST_LOCAL_PORT 258258

struct TestMessage : public SerializableSequence
{
    std::string str;

    TestMessage() {}

    TestMessage ( const std::string& str ) : str ( str ) {}

    MsgType getType() const override;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const override { ar ( str ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) override { ar ( str ); }
};

int RunAllTests ( int& argc, char *argv[] );
