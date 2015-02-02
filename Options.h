#pragma once

#include "Enum.h"
#include "Protocol.h"

#include <string>
#include <vector>
#include <unordered_map>


// Set of command line options
ENUM ( Options,
       // Regular options
       Help,
       GameDir,
       Tunnel,
       Training,
       Broadcast,
       Spectate,
       Offline,
       NoUi,
       Tourney,
       MaxDelay,
       DefaultRollback,
       Fullscreen,
       // Debug options
       Tests,
       Stdout,
       FakeUi,
       Dummy,
       StrictVersion,
       PidLog,
       SyncTest,
       Replay,
       // Special options
       NoFork,
       AppDir,
       SessionId );


// Forward declaration
namespace option { class Option; }


struct OptionsMessage : public SerializableSequence
{
    size_t operator[] ( const Options& opt ) const
    {
        auto it = options.find ( ( size_t ) opt.value );

        if ( it == options.end() )
            return 0;
        else
            return it->second.count;
    }

    void set ( const Options& opt, size_t count, const std::string& arg = "" )
    {
        if ( count == 0 )
            options.erase ( opt.value );
        else
            options[opt.value] = Opt ( count, arg );
    }

    const std::string& arg ( const Options& opt ) const
    {
        static const std::string EmptyString = "";

        auto it = options.find ( ( size_t ) opt.value );

        if ( it == options.end() )
            return EmptyString;
        else
            return it->second.arg;
    }

    OptionsMessage ( const std::vector<option::Option>& opt );

    PROTOCOL_MESSAGE_BOILERPLATE ( OptionsMessage, options )

private:

    struct Opt
    {
        size_t count;
        std::string arg;

        Opt() {}
        Opt ( size_t count, const std::string& arg = "" ) : count ( count ), arg ( arg ) {}

        CEREAL_CLASS_BOILERPLATE ( count, arg )
    };

    std::unordered_map<size_t, Opt> options;
};
