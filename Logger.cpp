#include "Logger.h"
#include "Version.h"

using namespace std;

void Logger::initialize ( const string& name, bool prependPidToName )
{
    if ( initialized )
        return;

    initialized = true;

    if ( name.empty() )
    {
        fd = stdout;
    }
    else
    {
        if ( prependPidToName )
            fd = fopen ( ( toString ( "log%08d", _getpid() ) + name ).c_str(), "w" );
        else
            fd = fopen ( name.c_str(), "w" );
    }

    fprintf ( fd, "COMMIT_ID %s\n", COMMIT_ID );
    fprintf ( fd, "BUILD_TIME %s\n", BUILD_TIME );
    fprintf ( fd, "VERSION %s\n", VERSION );
    fflush ( fd );
}

void Logger::deinitialize()
{
    if ( !initialized )
        return;

#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    fclose ( fd );
    fd = 0;
    initialized = false;
}

void Logger::flush()
{
#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    fflush ( fd );
}

void Logger::log ( const char *file, int line, const char *func, const char *message )
{
    if ( !fd )
        return;

    time_t t;
    time ( &t );
    tm *ts = gmtime ( &t );

#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    strftime ( buffer, sizeof ( buffer ), "%H:%M:%S", ts );

    string shortFunc ( func );
    shortFunc = shortFunc.substr ( 0, shortFunc.find ( '(' ) );

    fprintf ( fd, "%s:%s:%3d: %s : %s\n", buffer, file, line, shortFunc.c_str(), message );
    fflush ( fd );
}

Logger& Logger::get()
{
    static Logger logger;
    return logger;
}
