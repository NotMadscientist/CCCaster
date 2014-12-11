#include "Main.h"
#include "MainUi.h"
#include "Test.h"
#include "Exceptions.h"
#include "StringUtils.h"

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


static bool initDirsAndSanityCheck ( bool checkGameExe = true )
{
    bool success = true;

    char buffer[4096];

    appDir.clear();

    if ( GetModuleFileName ( GetModuleHandle ( 0 ), buffer, sizeof ( buffer ) ) )
    {
        appDir = buffer;
        appDir = appDir.substr ( 0, appDir.find_last_of ( "/\\" ) );

        replace ( appDir.begin(), appDir.end(), '/', '\\' );

        if ( !appDir.empty() && appDir.back() != '\\' )
            appDir += '\\';
    }

    DWORD val = GetFileAttributes ( ( appDir + FOLDER ).c_str() );

    if ( val == INVALID_FILE_ATTRIBUTES || ! ( val & FILE_ATTRIBUTE_DIRECTORY ) )
    {
        string folder = FOLDER;
        folder.pop_back();

        lastError += "\nMissing " + folder + " folder!";
        success = false;
    }

    val = GetFileAttributes ( ( appDir + HOOK_DLL ).c_str() );

    if ( val == INVALID_FILE_ATTRIBUTES )
    {
        lastError += "\nMissing " HOOK_DLL "!";
        success = false;
    }

    val = GetFileAttributes ( ( appDir + LAUNCHER ).c_str() );

    if ( val == INVALID_FILE_ATTRIBUTES )
    {
        lastError += "\nMissing " LAUNCHER "!";
        success = false;
    }

    if ( opt[Options::GameDir] && opt[Options::GameDir].arg )
    {
        ProcessManager::gameDir = opt[Options::GameDir].arg;

        replace ( ProcessManager::gameDir.begin(), ProcessManager::gameDir.end(), '/', '\\' );

        if ( ProcessManager::gameDir.back() != '\\' )
            ProcessManager::gameDir += '\\';
    }
    else
    {
        ProcessManager::gameDir = appDir;
    }

    if ( checkGameExe )
    {
        val = GetFileAttributes ( ( ProcessManager::gameDir + MBAA_EXE ).c_str() );

        if ( val == INVALID_FILE_ATTRIBUTES )
        {
            lastError += "\nCouldn't find " MBAA_EXE "!";
            success = false;
        }
    }

    return success;
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
            "Usage: " BINARY " [options] [port]\n"
            "       " BINARY " [options] [address] [port]\n"
            "       " BINARY " [options] [address]:[port]\n"
            "\n"
            "Options:"
        },

        { Options::Help,      0, "h",      "help", Arg::None,     "  --help, -h         Print help and exit.\n" },
        { Options::GameDir,   0, "d",       "dir", Arg::Required, "  --dir, -d folder   Specify game folder.\n" },

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
        },

        {
            Options::Tourney, 0, "", "tourney", Arg::None,
            "  --tourney          Tournament mode, forces just --offline and --no-ui.\n"
        },

#ifndef RELEASE
        { Options::Unknown,   0,  "",       "", Arg::None,        "Debug options:" },
        { Options::Tests,     0,  "",  "tests", Arg::None,        "  --tests            Run unit tests and exit." },
        { Options::Stdout,    0,  "", "stdout", Arg::None,        "  --stdout,          Output logs to stdout"},
        { Options::Tunnel,    0, "T", "tunnel", Arg::None,        "  --tunnel, -T       Force UDP tunnel" },
        { Options::Dummy,     0, "D",  "dummy", Arg::None,        "  --dummy, -D        Client mode with fake inputs" },
        { Options::PidLog,    0, "P", "pidlog", Arg::None,        "  --pidlog, -P       Tag log files with the PID" },
        { Options::FakeUi,    0, "F",   "fake", Arg::None,        "  --fake, -F         Fake UI mode\n" },

        {
            Options::StrictVersion, 0, "S", "strict", Arg::None,
            "  --strict, -S       Strict version match, can be stacked up to 3 times.\n"
            "                     -S means version suffix must match.\n"
            "                     -SS means commit ID must match.\n"
            "                     -SSS means build time must match.\n"
        },
#else
        { Options::Tunnel, 0, "", "tunnel", Arg::None, 0 },
        { Options::Dummy, 0, "", "dummy", Arg::None, 0 },
        { Options::PidLog, 0, "", "pidlog", Arg::None, 0 },
        { Options::StrictVersion, 0, "S", "", Arg::None, 0 },
#endif

        { Options::NoFork, 0, "", "no-fork", Arg::None, 0 }, // Don't fork when inside Wine, ie when under wineconsole

        {
            Options::Unknown, 0, "", "", Arg::None,
            "Examples:\n"
            "  " BINARY " 12345                 Host on port 12345\n"
            "  " BINARY " 12.34.56.78 12345     Connect to 12.34.56.78 on port 12345\n"
            "  " BINARY " 12.34.56.78:12345     Connect to 12.34.56.78 on port 12345\n"
            "  " BINARY " -b 12345              Broadcast on port 12345\n"
            "  " BINARY " -ot                   Offline training mode.\n"
            "  " BINARY " -o 4 -t               Offline training mode with 4 delay.\n"
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

    // Fork and re-run under wineconsole, needed for proper JLib support
    if ( ProcessManager::isWine() && !opt[Options::NoFork] )
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

    // Setup signal and console handlers
    signal ( SIGABRT, signalHandler );
    signal ( SIGINT, signalHandler );
    signal ( SIGTERM, signalHandler );
    SetConsoleCtrlHandler ( consoleCtrl, TRUE );

    // Initialize the main directories, this also does a sanity check
    if ( !initDirsAndSanityCheck ( !opt[Options::Dummy] ) )
    {
        PRINT ( "%s", trimmed ( lastError ) );
        PRINT ( "Press any key to exit." );
        system ( "@pause > nul" );
        return -1;
    }

    // Initialize logging
    if ( opt[Options::Stdout] )
        Logger::get().initialize();
    else if ( opt[Options::PidLog] )
        Logger::get().initialize ( appDir + LOG_FILE, LOG_DEFAULT_OPTIONS | PID_IN_FILENAME );
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

    // Initialize config
    ui.initialize();
    ui.initialConfig.mode.flags |= ( opt[Options::Training] && !opt[Options::Tourney] ? ClientMode::Training : 0 );

    if ( opt[Options::Spectate] )
        ui.initialConfig.mode.value = ClientMode::SpectateNetplay;

    RunFuncPtr run = ( opt[Options::FakeUi] ? runFake : runMain );

    // Warn on invalid command line opt
    for ( Option *it = opt[Options::Unknown]; it; it = it->next() )
        lastError += format ( "Unknown option: '%s'\n", it->name );

    // Non-opt 1 and 2 are the IP address and port
    for ( int i = 2; i < parser.nonOptionsCount(); ++i )
        lastError += format ( "Non-option (%d): '%s'\n", i, parser.nonOption ( i ) );

    if ( opt[Options::Offline] || opt[Options::Tourney] )
    {
        NetplayConfig netplayConfig;
        netplayConfig.mode.value = ClientMode::Offline;
        netplayConfig.mode.flags = ui.initialConfig.mode.flags;
        netplayConfig.delay = 0;
        netplayConfig.winCount = ui.getConfig().getInteger ( "versusWinCount" );
        netplayConfig.hostPlayer = 1;

        if ( opt[Options::Offline].arg && !opt[Options::Tourney] )
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
        netplayConfig.winCount = ui.getConfig().getInteger ( "versusWinCount" );
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

    if ( opt[Options::NoUi] || opt[Options::Tourney] )
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
        catch ( const Exception& exc )
        {
            PRINT ( "%s", exc.user );
        }
#ifdef NDEBUG
        catch ( const std::exception& exc )
        {
            PRINT ( "Error: %s", exc.what() );
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

namespace AsmHacks
{
// Empty definition for unused DLL callback
extern "C" void callback() {}
}
