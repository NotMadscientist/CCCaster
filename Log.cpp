#include "Log.h"

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
            fd = fopen ( ( toString ( "log%08d", _getpid() ) + name ).c_str(), "w" );
        else
            fd = fopen ( name.c_str(), "w" );

        time_t t;
        time ( &t );
        uint32_t id = _getpid() * t;

        fprintf ( fd, "ID %08x", id );
        fflush ( fd );
    }
}

void Log::log ( const char *file, int line, const char *func, const char *message )
{
    if ( !fd )
        return;

    time_t t;
    time ( &t );
    tm *ts = gmtime ( &t );

    LOCK ( mutex );

    strftime ( buffer, sizeof ( buffer ), "%H:%M:%S", ts );

    string shortFunc ( func );
    shortFunc = shortFunc.substr ( 0, shortFunc.find ( '(' ) );

    fprintf ( fd, "%s:%s:%d: %s : %s\n", buffer, file, line, shortFunc.c_str(), message );
    fflush ( fd );
}

void Log::close()
{
    LOCK ( mutex );
    fclose ( fd );
    fd = 0;
    isEnabled = false;
}

void Log::flush()
{
    LOCK ( mutex );
    fflush ( fd );
}
