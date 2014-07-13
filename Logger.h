#pragma once

#include "Thread.h"
#include "Utilities.h"

#include <string>
#include <cstdio>
#include <ctime>

class Logger
{
    // Log file
    FILE *fd;

    // Char buffer
    char buffer[4096];

    // Flag to indicate if initialized
    bool initialized;

    // Optionally mutexed logging
#ifdef LOGGER_MUTEXED
    Mutex mutex;
#endif

public:

    // Basic constructor
    inline Logger() : fd ( 0 ) , initialized ( false ) {}

    // Initialize / deinitialize logging
    void initialize ( const std::string& name = "", bool prependPidToName = false );
    void deinitialize();

    // Flush to file
    void flush();

    // Log a message with file, line, and function
    void log ( const char *file, int line, const char *func, const char *message );

    // Get the singleton instance
    static Logger& get();
};

#ifndef ENABLE_LOGGING

#define LOG(...)
#define LOG_LIST(...)

#else

#define LOG(FORMAT, ...)                                                                                    \
    do {                                                                                                    \
        Logger::get().log ( __FILE__, __LINE__, __PRETTY_FUNCTION__, TO_C_STR ( FORMAT, ## __VA_ARGS__ ) ); \
    } while ( 0 )

#define LOG_LIST(LIST, TO_STRING)                                                                           \
    do {                                                                                                    \
        std::string list;                                                                                   \
        for ( const auto& val : LIST )                                                                      \
            list += " " + TO_STRING ( val ) + ",";                                                          \
        if ( !LIST.empty() )                                                                                \
            list [ list.size() - 1 ] = ' ';                                                                 \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list );                                                     \
    } while ( 0 )

#endif
