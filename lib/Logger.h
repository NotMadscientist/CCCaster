#pragma once

#include "Thread.h"
#include "Utilities.h"

#include <string>
#include <cstdio>
#include <ctime>


#define PREPEND_PID     ( 0x01 )
#define LOG_TIMESTAMP   ( 0x02 )
#define LOG_FILE_LINE   ( 0x04 )
#define LOG_FUNC_NAME   ( 0x08 )


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
                      uint32_t options = ( LOG_TIMESTAMP | LOG_FILE_LINE | LOG_FUNC_NAME ) );
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

#endif // ENABLE_LOGGING


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


#if !defined(RELEASE) && defined(ENABLE_LOGGING)

#define ASSERT(ASSERTION)                                                                                           \
    do {                                                                                                            \
        if ( ASSERTION )                                                                                            \
            break;                                                                                                  \
        LOG ( "Assertion '%s' failed", #ASSERTION );                                                                \
        abort();                                                                                                    \
    } while ( 0 )

#else

#define ASSERT(...)

#endif
