#include "Logger.h"
#include "Version.h"

using namespace std;


void Logger::initialize ( const string& name, uint32_t options )
{
    if ( initialized )
        return;

    initialized = true;
    this->options = options;

    if ( name.empty() )
    {
        fd = stdout;
    }
    else
    {
        if ( options & PID_IN_FILENAME )
            fd = fopen ( ( toString ( "log%08d", _getpid() ) + name ).c_str(), "w" );
        else
            fd = fopen ( name.c_str(), "w" );
    }

    if ( options & LOG_VERSION )
    {
        fprintf ( fd, "COMMIT_ID %s\n", COMMIT_ID );
        fprintf ( fd, "BUILD_TIME %s\n", BUILD_TIME );
        fprintf ( fd, "VERSION %s\n\n", VERSION );
        fflush ( fd );
    }
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

#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    bool hasPrefix = false;

    if ( options & ( LOG_GM_TIME | LOG_LOCAL_TIME ) )
    {
        time_t t;
        time ( &t );

        tm *ts;
        if ( options & LOG_GM_TIME )
            ts = gmtime ( &t );
        else
            ts = localtime ( &t );

        strftime ( buffer, sizeof ( buffer ), "%H:%M:%S", ts );
        fprintf ( fd, "%s:", buffer );
        hasPrefix = true;
    }

    if ( options & LOG_FILE_LINE )
    {
        fprintf ( fd, "%s:%3d:", file, line );
        hasPrefix = true;
    }

    if ( options & LOG_FUNC_NAME )
    {
        string shortFunc ( func );
        shortFunc = shortFunc.substr ( 0, shortFunc.find ( '(' ) );
        fprintf ( fd, "%s:", shortFunc.c_str() );
        hasPrefix = true;
    }

    fprintf ( fd, ( hasPrefix ? " %s\n" : "%s\n" ), message );
    fflush ( fd );
}

Logger& Logger::get()
{
    static Logger logger;
    return logger;
}
