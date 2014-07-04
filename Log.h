#pragma once

#include "Thread.h"
#include "Util.h"

#include <string>
#include <cstdio>
#include <ctime>

class Log
{
    static char buffer[256];
    static FILE *fd;

#ifdef LOG_MUTEXED
    static Mutex mutex;
#endif

public:

    static bool isEnabled;

    static void open ( const std::string& name = "", bool prependPidToName = false );
    static void log ( const char *file, int line, const char *func, const char *message );
    static void close();
    static void flush();
};

#ifdef RELEASE

#define LOG(...)
#define LOG_LIST(...)

#else

#define LOG(FORMAT, ...)                                                                                    \
    do {                                                                                                    \
        if ( !Log::isEnabled ) break;                                                                       \
        Log::log ( __FILE__, __LINE__, __PRETTY_FUNCTION__, TO_C_STR ( FORMAT, ## __VA_ARGS__ ) );          \
    } while ( 0 )

#define LOG_LIST(LIST, TO_STRING)                                       \
    do {                                                                \
        if ( !Log::isEnabled )                                          \
            break;                                                      \
        string list;                                                    \
        for ( const auto& val : LIST )                                  \
            list += " " + TO_STRING ( val ) + ",";                      \
        if ( !LIST.empty() )                                            \
            list [ list.size() - 1 ] = ' ';                             \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list );                 \
    } while ( 0 )

#endif
