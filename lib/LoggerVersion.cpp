#include "Logger.h"
#include "Version.h"

using namespace std;


#ifdef DISABLE_LOGGING

void Logger::logVersion() {}

#else

void Logger::logVersion()
{
    fprintf ( fd, "LogId '%s'\n", logId.c_str() );
    fprintf ( fd, "Version '%s' { '%s', '%s', '%s' }\n", LocalVersion.code.c_str(),
              LocalVersion.major().c_str(), LocalVersion.minor().c_str(), LocalVersion.suffix().c_str() );
    fprintf ( fd, "Revision '%s' { isCustom=%d }\n", LocalVersion.revision.c_str(), LocalVersion.isCustom() );
    fprintf ( fd, "BuildTime '%s'\n", LocalVersion.buildTime.c_str() );

#if defined(DEBUG)
    fprintf ( fd, "BuildType 'debug'\n" );
#elif defined(LOGGING)
    fprintf ( fd, "BuildType 'logging'\n" );
#elif defined(RELEASE)
    fprintf ( fd, "BuildType 'release'\n" );
#else
    fprintf ( fd, "BuildType 'unknown'\n" );
#endif

    if ( !sessionId.empty() )
        fprintf ( fd, "SessionId '%s'\n", sessionId.c_str() );

    fflush ( fd );
}

#endif
