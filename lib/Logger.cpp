#include "Logger.h"
#include "Algorithms.h"
#include "TimerManager.h"

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
            const size_t i = file.find_last_of ( '.' );
            const string tmp = file.substr ( 0, i ) + format ( "_%08d", _getpid() ) + file.substr ( i );
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

    if ( !initialized )
        logId = generateRandomId();

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

        const uint64_t now = TimerManager::get().getNow ( true );

        fprintf ( fd, "%s.%03u:", buffer, ( uint32_t ) ( now % 1000 ) );
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
