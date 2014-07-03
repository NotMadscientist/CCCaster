#pragma once

#include "Thread.h"
#include "Util.h"

#include <string>
#include <cstdio>
#include <ctime>

class Log
{
    static char buffer[256];
    static Mutex mutex;
    static FILE *fd;

public:

    static bool isEnabled;

    static void open ( const std::string& name = "", bool prependPidToName = false );
    static void log ( const char *file, int line, const char *func, const char *message );
    static void close();
    static void flush();
};

#define LOG(FORMAT, ...)                                                                                    \
    do {                                                                                                    \
        if ( !Log::isEnabled ) break;                                                                       \
        Log::log ( __FILE__, __LINE__, __PRETTY_FUNCTION__, TO_C_STR ( FORMAT, ## __VA_ARGS__ ) );          \
    } while ( 0 )
