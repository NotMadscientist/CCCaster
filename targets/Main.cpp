#include "Main.h"
#include "MainUi.h"
#include "Test.h"

#include <optionparser.h>
#include <windows.h>

using namespace std;
using namespace option;


#define LOG_FILE FOLDER "debug.log"


vector<option::Option> opt;

MainUi ui;

string lastError;

string appDir;


void runMain ( const IpAddrPort& address, const Serializable& config );
void runFake ( const IpAddrPort& address, const Serializable& config );


static void initAppDir()
{
    char buffer[4096];

    appDir.clear();

    if ( GetModuleFileName ( 0, buffer, sizeof ( buffer ) ) )
    {
        appDir = buffer;
        appDir = appDir.substr ( 0, appDir.find_last_of ( "/\\" ) );

        if ( detectWine() )
        {
            replace ( appDir.begin(), appDir.end(), '\\', '/' );
            if ( !appDir.empty() && appDir.back() != '/' )
                appDir += '/';
        }
        else
        {
            replace ( appDir.begin(), appDir.end(), '/', '\\' );
            if ( !appDir.empty() && appDir.back() != '\\' )
                appDir += '\\';
        }
    }
}

static void deinitialize()
{
    static bool deinitialized = false;
    static Mutex deinitMutex;

    LOCK ( deinitMutex );

    if ( deinitialized )
        return;
    deinitialized = true;

    EventManager::get().release();
    Logger::get().deinitialize();
    exit ( 0 );
}

static void signalHandler ( int signum )
{
    LOG ( "Interrupt signal %d received", signum );
    deinitialize();
}

static BOOL WINAPI consoleCtrl ( DWORD ctrl )
{
    LOG ( "Console ctrl %d received", ctrl );
    deinitialize();
    return TRUE;
}


int main ( int argc, char *argv[] )
{
#if 0
    // Protocol testing code
    Logger::get().initialize();

    size_t pos = 0, consumed;
    char bytes[] =
    {
    };

    for ( ;; )
    {
        if ( pos == sizeof ( bytes ) )
            break;

        MsgPtr msg = Protocol::decode ( &bytes[pos], sizeof ( bytes ) - pos, consumed );

        pos += consumed;

        PRINT ( "%s", msg );

        if ( !msg.get() )
            break;
    }

    Logger::get().deinitialize();
    return 0;
#endif

    static const Descriptor desc[] =
    {
        {
            Options::Unknown, 0, "", "", Arg::None,
            "Usage: " BINARY " [options] [address] [port]\n"
            "\n"
            "Options:"
        },

        { Options::Help,      0, "h",      "help", Arg::None,     "  --help, -h         Print help and exit.\n" },
        { Options::GameDir,   0, "d",       "dir", Arg::Required, "  --dir, -d folder   Specify game folder.\n" },
        { Options::Tunnel,    0, "T",    "tunnel", Arg::None,     "  --tunnel, -T       Connect via UDP tunnel.\n" },

        { Options::Training,  0, "t",  "training", Arg::None,     "  --training, -t     Force training mode." },
        { Options::Broadcast, 0, "b", "broadcast", Arg::None,     "  --broadcast, -b    Force broadcast mode." },
        { Options::Spectate,  0, "s",  "spectate", Arg::None,     "  --spectate, -s     Force spectator mode.\n" },

        {
            Options::Offline, 0, "o", "offline", Arg::OptionalNumeric,
            "  --offline, -o D    Force offline mode.\n"
            "                     D is the optional delay, defaults to 0.\n"
        },

        {
            Options::NoUi, 0, "n", "no-ui", Arg::None,
            "  --no-ui, -n        No UI, just quits after running once.\n"
            "                     Should be used with address and/or port.\n"
        },

        {
            Options::Tournament, 0, "", "tournament", Arg::None,
            "  --tournament       Tournament mode, forces just --offline and --no-ui.\n"
        },

#ifndef RELEASE
        { Options::Unknown,   0,  "",       "", Arg::None,        "Debug options:" },
        { Options::Tests,     0,  "",  "tests", Arg::None,        "  --tests            Run unit tests and exit." },
        { Options::Stdout,    0,  "", "stdout", Arg::None,        "  --stdout,          Output logs to stdout"},
        { Options::FakeUi,    0, "F",   "fake", Arg::None,        "  --fake, -F         Fake UI mode" },
        { Options::Dummy,     0, "D",  "dummy", Arg::None,        "  --dummy, -D        Client mode with fake inputs" },
        { Options::CheckSync, 0, "C",  "check", Arg::None,        "  --check, -C        Check for desyncs\n" },

        {
            Options::Strict, 0, "S", "strict", Arg::None,
            "  --strict, -S       Strict version match, can be stacked up to 3 times.\n"
            "                     -S means version suffix must match.\n"
            "                     -SS means commit ID must match.\n"
            "                     -SSS means build time must match.\n"
        },
#endif

        { Options::NoFork, 0, "", "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie when under wineconsole

        {
            Options::Unknown, 0, "", "", Arg::None,
            "Examples:\n"
            "  " BINARY " --unknown -- --this_is_no_option\n"
            "  " BINARY " -unk --plus -ppp file1 file2\n"
        },

        { 0, 0, 0, 0, 0, 0 }
    };

    string argv0;

    // Skip argv[0] if present, because optionparser doesn't like it
    if ( argc > 0 )
    {
        argv0 = argv[0];
        --argc;
        ++argv;
    }

    Stats stats ( desc, argc, argv );
    Option buffer[stats.buffer_max];
    opt.resize ( stats.options_max );
    Parser parser ( desc, argc, argv, &opt[0], buffer );

    if ( parser.error() )
        return -1;

    if ( opt[Options::Help] )
    {
        printUsage ( cout, desc );
        return 0;
    }

    // Setup signal and console handlers
    signal ( SIGABRT, signalHandler );
    signal ( SIGINT, signalHandler );
    signal ( SIGTERM, signalHandler );
    SetConsoleCtrlHandler ( consoleCtrl, TRUE );

    // Initialize the main application directory
    initAppDir();

    // Initialize logging
    if ( opt[Options::Stdout] )
        Logger::get().initialize();
    else
        Logger::get().initialize ( appDir + LOG_FILE );

    LOG ( "Running from: %s", appDir );

    // Log parsed command line opt
    for ( size_t i = 0; i < opt.size(); ++i )
    {
        if ( opt[i] )
            LOG ( "%s", Options ( ( Options::Enum ) i ) );
        if ( opt[i].arg )
            LOG ( "arg='%s'", opt[i].arg );
    }

#ifndef RELEASE
    // Run the unit test suite and exit
    if ( opt[Options::Tests] )
    {
        int result = RunAllTests ( argc, argv );
        Logger::get().deinitialize();
        return result;
    }
#endif

    // Fork and re-run under wineconsole, needed for proper JLib support
    if ( detectWine() && !opt[Options::NoFork] )
    {
        string cmd = "wineconsole " + argv0 + " --no-fork";

        for ( int i = 0; i < argc; ++i )
        {
            cmd += " ";
            cmd += argv[i];
        }

        PRINT ( "%s", cmd );
        return system ( cmd.c_str() );
    }

    // Initialize game dir first because ui.initialize() needs it
    if ( opt[Options::GameDir] && opt[Options::GameDir].arg )
    {
        ProcessManager::gameDir = opt[Options::GameDir].arg;
        if ( ProcessManager::gameDir.back() != '/' && ProcessManager::gameDir.back() != '\\' )
            ProcessManager::gameDir += '/';
    }

    // Initialize config
    ui.initialize();
    ui.initialConfig.mode.flags |= ( opt[Options::Training] ? ClientMode::Training : 0 );

    if ( opt[Options::Spectate] )
        ui.initialConfig.mode.value = ClientMode::Spectate;

    RunFuncPtr run = ( opt[Options::FakeUi] ? runFake : runMain );

    // Warn on invalid command line opt
    for ( Option *it = opt[Options::Unknown]; it; it = it->next() )
        lastError += toString ( "Unknown option: '%s'\n", it->name );

    // Non-opt 1 and 2 are the IP address and port
    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        lastError += toString ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    if ( opt[Options::Offline] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.mode.value = ClientMode::Offline;
        netplayConfig.mode.flags = ui.initialConfig.mode.flags;
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;

        if ( opt[Options::Offline].arg )
        {
            uint32_t delay = 0;
            stringstream ss ( opt[Options::Offline].arg );

            if ( ( ss >> delay ) && ( delay < 0xFF ) )
                netplayConfig.delay = delay;
        }

        run ( "", netplayConfig );
    }
    else if ( opt[Options::Broadcast] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.mode.value = ClientMode::Broadcast;
        netplayConfig.mode.flags = ui.initialConfig.mode.flags;
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;

        stringstream ss;
        if ( parser.nonOptionsCount() == 1 )
            ss << parser.nonOption ( 0 );
        else if ( parser.nonOptionsCount() == 2 )
            ss << parser.nonOption ( 1 );

        if ( ! ( ss >> netplayConfig.broadcastPort ) )
            netplayConfig.broadcastPort = 0;

        run ( "", netplayConfig );
    }
    else if ( parser.nonOptionsCount() == 1 )
    {
        IpAddrPort address = parser.nonOption ( 0 );

        if ( ui.initialConfig.mode.value == ClientMode::Unknown )
            ui.initialConfig.mode.value = ( address.addr.empty() ? ClientMode::Host : ClientMode::Client );

        run ( address, ui.initialConfig );
    }
    else if ( parser.nonOptionsCount() == 2 )
    {
        IpAddrPort address = string ( parser.nonOption ( 0 ) ) + ":" + parser.nonOption ( 1 );

        if ( ui.initialConfig.mode.value == ClientMode::Unknown )
            ui.initialConfig.mode.value = ( address.addr.empty() ? ClientMode::Host : ClientMode::Client );

        run ( address, ui.initialConfig );
    }
    else if ( opt[Options::NoUi] )
    {
        printUsage ( cout, desc );
        return 0;
    }

    if ( opt[Options::NoUi] )
    {
        if ( !lastError.empty() )
            PRINT ( "%s", lastError );
    }
    else
    {
        if ( !lastError.empty() )
            ui.sessionError = lastError;

        try
        {
            ui.main ( run );
        }
        catch ( const Exception& err )
        {
            PRINT ( "Error: %s", err );
        }
#ifdef NDEBUG
        catch ( const std::exception& err )
        {
            PRINT ( "Error: %s", err.what() );
        }
        catch ( ... )
        {
            PRINT ( "Unknown error!" );
        }
#endif
    }

    LOG ( "lastError='%s'", lastError );

    deinitialize();
    return 0;
}


// This is here because only MainApp creates and sends OptionsMessages
OptionsMessage::OptionsMessage ( const std::vector<option::Option>& opt )
{
    for ( size_t i = 0; i < opt.size(); ++i )
    {
        if ( opt[i] && opt[i].arg )
            options[i] = Opt ( opt[i].count(), opt[i].arg );
        else if ( opt[i] )
            options[i] = Opt ( opt[i].count() );
    }
}

// Empty definition for unused DLL callback
extern "C" void callback() {}
