#pragma once

#include "Thread.h"
#include "StringUtils.h"

#include <string>
#include <cstdio>
#include <ctime>


#define LOG_VERSION     ( 0x01 )    // Log version information at the beginning
#define LOG_GM_TIME     ( 0x02 )    // Log the gmtime timestamp per message
#define LOG_LOCAL_TIME  ( 0x04 )    // Log the localtime timestamp per message
#define LOG_FILE_LINE   ( 0x08 )    // Log file:line per message
#define LOG_FUNC_NAME   ( 0x10 )    // Log the function name per message
#define PID_IN_FILENAME ( 0x20 )    // Append the PID to the log filename


class Logger
{
    // Log file path
    std::string file;

    // Log identifier, to match up reinitialized logs
    std::string logId;

    // Bit mask of options
    uint32_t options = 0;

    // Log file descriptor
    FILE *fd = 0;

    // Char buffer
    char buffer[4096];

    // Flag to indicate if initialized
    bool initialized = false;

    // Optionally mutexed logging
#ifdef LOGGER_MUTEXED
    Mutex mutex;
#endif

public:

    // Session ID
    std::string sessionId;

    // Basic constructor
    Logger() {}

    // Initialize / deinitialize logging
    void initialize ( const std::string& file = "",
                      uint32_t options = ( LOG_VERSION | LOG_GM_TIME | LOG_FILE_LINE | LOG_FUNC_NAME
// #ifndef RELEASE
//                                            | PID_IN_FILENAME
// #endif
                                         ) );
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

#define LOG_TO(LOGGER, FORMAT, ...)                                                                                    \
    do {                                                                                                               \
        LOGGER.log ( __BASE_FILE__, __LINE__, __PRETTY_FUNCTION__, format ( FORMAT, ## __VA_ARGS__ ).c_str() );        \
    } while ( 0 )

#define LOG(FORMAT, ...)                                                                                               \
    do {                                                                                                               \
        Logger::get().log ( __BASE_FILE__, __LINE__, __PRETTY_FUNCTION__, format ( FORMAT, ## __VA_ARGS__ ).c_str() ); \
    } while ( 0 )

#define LOG_LIST(LIST, TO_STRING)                                                                                      \
    do {                                                                                                               \
        std::string list;                                                                                              \
        for ( const auto& val : LIST )                                                                                 \
            list += " " + TO_STRING ( val ) + ",";                                                                     \
        if ( !LIST.empty() )                                                                                           \
            list [ list.size() - 1 ] = ' ';                                                                            \
        LOG ( "this=%08x; "#LIST "=[%s]", this, list );                                                                \
    } while ( 0 )

#endif // DISABLE_LOGGING


#ifdef DISABLE_ASSERTS

#define ASSERT(...)

#else

#define ASSERT(ASSERTION)                                                                                              \
    do {                                                                                                               \
        if ( ASSERTION )                                                                                               \
            break;                                                                                                     \
        LOG ( "Assertion '%s' failed", #ASSERTION );                                                                   \
        PRINT ( "Assertion '%s' failed", #ASSERTION );                                                                 \
        abort();                                                                                                       \
    } while ( 0 )

#endif // DISABLE_ASSERTS


#define ASSERT_IMPOSSIBLE           ASSERT ( !"This shouldn't happen!" )
#define ASSERT_UNIMPLEMENTED        ASSERT ( !"Unimplemented!" )
