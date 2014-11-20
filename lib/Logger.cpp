#include "Logger.h"
#include "Version.h"
#include "Algorithms.h"

using namespace std;


#ifdef DISABLE_LOGGING

void Logger::initialize ( const string& name, uint32_t options ) {}
void Logger::deinitialize() {}
void Logger::flush() {}
void Logger::log ( const char *file, int line, const char *func, const char *message ) {}

#else

void Logger::initialize ( const string& file, uint32_t options )
{
#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    bool same = initialized && ( this->file == file );

    this->options = options;

    if ( file.empty() )
    {
        this->file.clear();
        fd = stdout;
    }
    else
    {
        if ( options & PID_IN_FILENAME )
        {
            const string tmp = format ( "log%08d", _getpid() ) + file;
            same = ( this->file == tmp );
            this->file = tmp;
        }
        else
        {
            this->file = file;
        }

        // Reopen the file if the path changed
        if ( fd && !same )
        {
            fflush ( fd );
            fclose ( fd );
        }

        // Append to the file if the path is the same
        fd = fopen ( this->file.c_str(), same ? "a" : "w" );
    }

    // Only log version if the path changed
    if ( ( options & LOG_VERSION ) && !same )
    {
        if ( !initialized )
            logId = generateRandomId();

        fprintf ( fd, "LogId '%s'\n", logId.c_str() );
        fprintf ( fd, "Version '%s' { '%s', '%s', '%s' }\n", LocalVersion.code.c_str(),
                  LocalVersion.major().c_str(), LocalVersion.minor().c_str(), LocalVersion.suffix().c_str() );
        fprintf ( fd, "CommitId '%s' { isCustom=%d }\n", LocalVersion.commitId.c_str(), LocalVersion.isCustom() );
        fprintf ( fd, "BuildTime '%s'\n", LocalVersion.buildTime.c_str() );

        if ( !sessionId.empty() )
            fprintf ( fd, "SessionId '%s'\n", sessionId.c_str() );

        fflush ( fd );
    }

    initialized = true;
}

void Logger::deinitialize()
{
    if ( !initialized )
        return;

#ifdef LOGGER_MUTEXED
    LOCK ( mutex );
#endif

    if ( fd && fd != stdout )
        fclose ( fd );

    file.clear();
    logId.clear();
    sessionId.clear();
    options = 0;
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

#endif // DISABLE_LOGGING

Logger& Logger::get()
{
    static Logger instance;
    return instance;
}
