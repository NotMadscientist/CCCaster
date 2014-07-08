#include "Log.h"
#include "Event.h"
#include "Test.h"

#include <optionparser.h>

#define LOG_FILE "debug.log"

using namespace std;
using namespace option;

enum optionIndex { UNKNOWN, HELP, TEST, PLUS };

static const Descriptor options[] =
{
    { UNKNOWN, 0, "", "", Arg::None, "Usage: " BINARY " [options]\n\nOptions:" },
    { HELP,    0, "h", "help", Arg::None, "  --help, -h    Print usage and exit." },
    { TEST,    0, "t", "test", Arg::None, "  --test, -t    Run unit tests and exit." },
    { PLUS,    0, "p", "plus", Arg::None, "  --plus, -p    Increment count." },
    {
        UNKNOWN, 0, "",  "", Arg::None,
        "\nExamples:\n"
        "  " BINARY " --unknown -- --this_is_no_option\n"
        "  " BINARY " -unk --plus -ppp file1 file2\n"
    },
    { 0, 0, 0, 0, 0, 0 }
};

int main ( int argc, char *argv[] )
{
    // skip program name argv[0] if present
    argc -= ( argc > 0 );
    argv += ( argc > 0 );

    Stats stats ( options, argc, argv );
    Option opt[stats.options_max], buffer[stats.buffer_max];
    Parser parser ( options, argc, argv, opt, buffer );

    if ( parser.error() )
        return -1;

    if ( opt[HELP] || argc == 0 )
    {
        printUsage ( cout, options );
        return 0;
    }

    if ( opt[TEST] )
    {
        Log::get().initialize();
        EventManager::get().initialize();
        int result = RunAllTests ( argc, argv );
        EventManager::get().deinitialize();
        Log::get().deinitialize();
        return result;
    }

    for ( Option *it = opt[UNKNOWN]; it; it = it->next() )
        cout << "Unknown option: " << it->name << endl;

    for ( int i = 0; i < parser.nonOptionsCount(); ++i )
        cout << "Non-option #" << i << ": " << parser.nonOption ( i ) << endl;

    return 0;
}
