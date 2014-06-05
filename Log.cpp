#include "Log.h"
#include "Util.h"

#include <cstdarg>

using namespace std;

char Log::buffer[256];

Mutex Log::mutex;

FILE *Log::fd = 0;

bool Log::isEnabled = false;

void Log::open ( const string& name, bool prependPidToName )
{
    if ( isEnabled )
        return;

    isEnabled = true;

    if ( name.empty() )
    {
        fd = stdout;
    }
    else
    {
        if ( prependPidToName )
            fd = fopen ( ( "log" + toString ( "%08d", _getpid() ) + name ).c_str(), "w" );
        else
            fd = fopen ( name.c_str(), "w" );

        time_t t;
        time ( &t );
        uint32_t id = _getpid() * t;

        fprintf ( fd, "ID %08x", id );
        fflush ( fd );
    }
}

void Log::log ( const char *file, int line, const char *func, const char *format, ... )
{
    time_t t;
    time ( &t );
    tm *ts = localtime ( &t );

    LOCK ( mutex );

    strftime ( buffer, sizeof ( buffer ), "%H:%M:%S", ts );
    fprintf ( fd, "%s:%s:%d: %s; ", buffer, file, line, func );

    va_list args;
    va_start ( args, format );
    vfprintf ( fd, format, args );
    va_end ( args );

    fprintf ( Log::fd, "\n" );
    fflush ( fd );
}

void Log::close()
{
    LOCK ( mutex );
    fclose ( fd );
}

void Log::flush()
{
    LOCK ( mutex );
    fflush ( fd );
}
