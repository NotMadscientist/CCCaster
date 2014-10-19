#pragma once

#include "Thread.h"
#include "Utilities.h"

#include <string>
#include <cstdio>
#include <ctime>


#define LOG_VERSION     ( 0x01 )    // Log version information at the beginning
#define LOG_GM_TIME     ( 0x02 )    // Log the gmtime timestamp per message
#define LOG_LOCAL_TIME  ( 0x04 )    // Log the localtime timestamp per message
#define LOG_FILE_LINE   ( 0x08 )    // Log file:line per message
#define LOG_FUNC_NAME   ( 0x10 )    // Log the function name per message
#define PID_IN_FILENAME ( 0x20 )    // Prepend the PID in the log filename


class Logger
{
    // Log file
    FILE *fd = 0;

    // Char buffer
    char buffer[4096];

    // Flag to indicate if initialized
    bool initialized = false;

    // Bit mask of options
    uint32_t options = 0;

    // Optionally mutexed logging
#ifdef LOGGER_MUTEXED
    Mutex mutex;
#endif

public:

    // Basic constructor
    Logger() {}

    // Initialize / deinitialize logging
    void initialize ( const std::string& name = "",
                      uint32_t options = ( LOG_VERSION | LOG_GM_TIME | LOG_FILE_LINE | LOG_FUNC_NAME ) );
    void deinitialize();

    // Flush to file
    void flush();

    // Log a message with file, line, and function
    void log ( const char *file, int line, const char *func, const char *message );

    // Get the singleton instance
    static Logger& get();
};


#ifdef DISABLE_LOGGING

#define LOG_TO(...)
#define LOG(...)
#define LOG_LIST(...)

#else

#define LOG_TO(LOGGER, FORMAT, ...)                                                                                 \
    do {                                                                                                            \
        LOGGER.log ( __BASE_FILE__, __LINE__, __PRETTY_FUNCTION__, TO_C_STR ( FORMAT, ## __VA_ARGS__ ) );           \
    } while ( 0 )

#define LOG(FORMAT, ...)                                                                                            \
    do {                                                                                                            \
        Logger::get().log ( __BASE_FILE__, __LINE__, __PRETTY_FUNCTION__, TO_C_STR ( FORMAT, ## __VA_ARGS__ ) );    \
    } while ( 0 )

#define LOG_LIST(LIST, TO_STRING)                                                                                   \
    do {                                                                                                            \
        std::string list;                                                                                           \
        for ( const auto& val : LIST )                                                                              \
            list += " " + TO_STRING ( val ) + ",";                                                                  \
        if ( !LIST.empty() )                                                                                        \
            list [ list.size() - 1 ] = ' ';                                                                         \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list );                                                             \
    } while ( 0 )

#endif // DISABLE_LOGGING


#ifndef RELEASE

#define ASSERT(ASSERTION)                                                                                           \
    do {                                                                                                            \
        if ( ASSERTION )                                                                                            \
            break;                                                                                                  \
        LOG ( "Assertion '%s' failed", #ASSERTION );                                                                \
        abort();                                                                                                    \
    } while ( 0 )

#else

#define ASSERT(...)

#endif // RELEASE


#define LOG_AND_THROW_STRING(FORMAT, ...)                                                                           \
    do {                                                                                                            \
        ::Exception err = toString ( FORMAT, ## __VA_ARGS__ );                                                      \
        LOG ( FORMAT, ## __VA_ARGS__ );                                                                             \
        throw err;                                                                                                  \
    } while ( 0 )


#define LOG_AND_THROW_ERROR(EXCEPTION, FORMAT, ...)                                                                 \
    do {                                                                                                            \
        LOG ( "%s; " FORMAT, EXCEPTION, ## __VA_ARGS__ );                                                           \
        throw EXCEPTION;                                                                                            \
    } while ( 0 )


#define ASSERT_IMPOSSIBLE           ASSERT ( !"This shouldn't happen!" )
#define LOG_AND_THROW_IMPOSSIBLE    LOG_AND_THROW_STRING ( !"This shouldn't happen!" )
