#include "Logger.hpp"
#include "Algorithms.hpp"
#include "TimerManager.hpp"

using namespace std;


#ifdef DISABLE_LOGGING

void Logger::initialize ( const string& filePath, uint32_t _options ) {}
void Logger::deinitialize() {}
void Logger::flush() {}
void Logger::log ( const char *srcFile, int srcLine, const char *srcFunc, const char *logMessage ) {}

#else

void Logger::initialize ( const string& filePath, uint32_t _options )
{
#ifdef LOGGER_MUTEXED
    LOCK ( _mutex );
#endif

    bool same = _initialized && ( _filePath == filePath );

    _options = _options;

    if ( filePath.empty() )
    {
        _filePath.clear();
        _fd = stdout;
    }
    else
    {
        if ( _options & PID_IN_FILENAME )
        {
            const size_t i = filePath.find_last_of ( '.' );
            const string tmp = filePath.substr ( 0, i ) + format ( "_%08d", _getpid() ) + filePath.substr ( i );
            same = ( _filePath == tmp );
            _filePath = tmp;
        }
        else
        {
            _filePath = filePath;
        }

        // Reopen the file if the path changed
        if ( _fd && !same )
        {
            fflush ( _fd );
            fclose ( _fd );
        }

        // Append to the file if the path is the same
        _fd = fopen ( _filePath.c_str(), same ? "a" : "w" );
    }

    if ( ! _initialized )
        _logId = generateRandomId();

    _initialized = true;
}

void Logger::deinitialize()
{
    if ( ! _initialized )
        return;

#ifdef LOGGER_MUTEXED
    LOCK ( _mutex );
#endif

    if ( _fd && _fd != stdout )
        fclose ( _fd );

    sessionId.clear();
    _filePath.clear();
    _logId.clear();
    _options = 0;
    _fd = 0;
    _initialized = false;
}

void Logger::flush()
{
#ifdef LOGGER_MUTEXED
    LOCK ( _mutex );
#endif

    fflush ( _fd );
}

void Logger::log ( const char *srcFile, int srcLine, const char *srcFunc, const char *logMessage )
{
    if ( ! _fd )
        return;

#ifdef LOGGER_MUTEXED
    LOCK ( _mutex );
#endif

    bool hasPrefix = false;

    if ( _options & ( LOG_GM_TIME | LOG_LOCAL_TIME ) )
    {
        time_t t;
        time ( &t );

        tm *ts;
        if ( _options & LOG_GM_TIME )
            ts = gmtime ( &t );
        else
            ts = localtime ( &t );

        strftime ( _buffer, sizeof ( _buffer ), "%H:%M:%S", ts );

        const uint64_t now = TimerManager::get().getNow ( true );

        fprintf ( _fd, "%s.%03u:", _buffer, ( uint32_t ) ( now % 1000 ) );
        hasPrefix = true;
    }

    if ( _options & LOG_FILE_LINE )
    {
        fprintf ( _fd, "%s:%3d:", srcFile, srcLine );
        hasPrefix = true;
    }

    if ( _options & LOG_FUNC_NAME )
    {
        string shortFunc ( srcFunc );
        shortFunc = shortFunc.substr ( 0, shortFunc.find ( '(' ) );
        fprintf ( _fd, "%s:", shortFunc.c_str() );
        hasPrefix = true;
    }

    fprintf ( _fd, ( hasPrefix ? " %s\n" : "%s\n" ), logMessage );
    fflush ( _fd );
}

#endif // DISABLE_LOGGING

Logger& Logger::get()
{
    static Logger instance;
    return instance;
}
