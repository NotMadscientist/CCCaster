#include "Logger.hpp"
#include "Version.hpp"

using namespace std;


#ifdef DISABLE_LOGGING

void Logger::logVersion() {}

#else

void Logger::logVersion()
{
    fprintf ( _fd, "LogId '%s'\n", _logId.c_str() );
    fprintf ( _fd, "Version '%s' { '%s', '%s', '%s' }\n", LocalVersion.code.c_str(),
              LocalVersion.major().c_str(), LocalVersion.minor().c_str(), LocalVersion.suffix().c_str() );
    fprintf ( _fd, "Revision '%s' { isCustom=%d }\n", LocalVersion.revision.c_str(), LocalVersion.isCustom() );
    fprintf ( _fd, "BuildTime '%s'\n", LocalVersion.buildTime.c_str() );

#if defined(DEBUG)
    fprintf ( _fd, "BuildType 'debug'\n" );
#elif defined(LOGGING)
    fprintf ( _fd, "BuildType 'logging'\n" );
#elif defined(RELEASE)
    fprintf ( _fd, "BuildType 'release'\n" );
#else
    fprintf ( _fd, "BuildType 'unknown'\n" );
#endif

    if ( ! sessionId.empty() )
        fprintf ( _fd, "SessionId '%s'\n", sessionId.c_str() );

    fflush ( _fd );
}

#endif
