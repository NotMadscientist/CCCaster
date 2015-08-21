#include "MainUpdater.hpp"
#include "Logger.hpp"
#include "ProcessManager.hpp"

#include <vector>
#include <unordered_set>

#include <windows.h>

using namespace std;


// Path of the latest version file
#define LATEST_VERSION_PATH "LatestVersion"

// Main update archive file name
#define UPDATE_ARCHIVE_FILE "update.zip"

// Timeout for update version check
#define VERSION_CHECK_TIMEOUT ( 1000 )


static const vector<string> updateServers =
{
    "http://192.210.227.23/",
    "http://104.206.199.123/",
};


MainUpdater::MainUpdater ( Owner *owner ) : owner ( owner )
{
    if ( ProcessManager::isWine() )
    {
        _downloadDir = ProcessManager::appDir;
        return;
    }

    char buffer[4096];
    if ( GetTempPath ( sizeof ( buffer ), buffer ) )
        _downloadDir = normalizeWindowsPath ( buffer );

    if ( _downloadDir.empty() )
        _downloadDir = ProcessManager::appDir;
}

void MainUpdater::fetch ( const Type& type )
{
    _type = type;

    _currentServerIdx = 0;

    doFetch ( type );
}

void MainUpdater::doFetch ( const Type& type )
{
    string url = updateServers[_currentServerIdx];

    switch ( type.value )
    {
        case Type::Version:
            url += LATEST_VERSION_PATH;
            _httpGet.reset ( new HttpGet ( this, url, VERSION_CHECK_TIMEOUT ) );
            _httpGet->start();
            break;

        case Type::ChangeLog:
            url += CHANGELOG;
            _httpDownload.reset ( new HttpDownload ( this, url, _downloadDir + CHANGELOG ) );
            _httpDownload->start();
            break;

        case Type::Archive:
            if ( _latestVersion.empty() )
            {
                LOG ( "Latest version is unknown" );

                if ( owner )
                    owner->fetchFailed ( this, Type::Archive );
                return;
            }

            url += format ( "cccaster.v%s.zip", _latestVersion.code );
            _httpDownload.reset ( new HttpDownload ( this, url, _downloadDir + UPDATE_ARCHIVE_FILE ) );
            _httpDownload->start();
            break;

        default:
            break;
    }
}

bool MainUpdater::openChangeLog() const
{
    unordered_set<string> folders = { _downloadDir, ProcessManager::appDir };

    for ( const string& folder : folders )
    {
        const DWORD val = GetFileAttributes ( ( folder + CHANGELOG ).c_str() );

        if ( val != INVALID_FILE_ATTRIBUTES )
        {
            if ( ProcessManager::isWine() )
                system ( ( "notepad " + folder + CHANGELOG ).c_str() );
            else
                system ( ( "\"start \"Viewing change log\" notepad " + folder + CHANGELOG + "\"" ).c_str() );
            return true;
        }

        LOG ( "Missing: %s", folder + CHANGELOG );
    }

    LOG ( "Could not open any change logs" );

    return false;
}

bool MainUpdater::extractArchive() const
{
    DWORD val = GetFileAttributes ( ( _downloadDir + CHANGELOG ).c_str() );

    if ( val == INVALID_FILE_ATTRIBUTES )
    {
        LOG ( "Missing: %s", _downloadDir + CHANGELOG );
        return false;
    }

    if ( _latestVersion.empty() )
    {
        LOG ( "Latest version is unknown" );
        return false;
    }

    const string srcUpdater = ProcessManager::appDir + FOLDER + UPDATER;
    string tmpUpdater = _downloadDir + UPDATER;

    if ( srcUpdater != tmpUpdater && ! CopyFile ( srcUpdater.c_str(), tmpUpdater.c_str(), FALSE ) )
        tmpUpdater = srcUpdater;

    const string binary = format ( "cccaster.v%s.%s.exe", _latestVersion.major(), _latestVersion.minor() );

    const string command = format ( "\"" + tmpUpdater + "\" %d %s %s %s",
                                    GetCurrentProcessId(),
                                    binary,
                                    _downloadDir + UPDATE_ARCHIVE_FILE,
                                    ProcessManager::appDir );

    LOG ( "Binary: %s", binary );

    LOG ( "Command: %s", command );

    system ( ( "\"start \"Updating...\" " + command + "\"" ).c_str() );

    exit ( 0 );

    return true;
}

void MainUpdater::httpResponse ( HttpGet *httpGet, int code, const string& data, uint32_t remainingBytes )
{
    ASSERT ( _httpGet.get() == httpGet );
    ASSERT ( _type == Type::Version );

    Version version ( trimmed ( data ) );

    _httpGet.reset();

    if ( code != 200 || version.major().empty() || version.minor().empty() )
    {
        httpFailed ( httpGet );
        return;
    }

    _httpGet.reset();

    _latestVersion = version;

    if ( owner )
        owner->fetchCompleted ( this, Type::Version );
}

void MainUpdater::httpFailed ( HttpGet *httpGet )
{
    ASSERT ( _httpGet.get() == httpGet );
    ASSERT ( _type == Type::Version );

    _httpGet.reset();

    ++_currentServerIdx;

    if ( _currentServerIdx >= updateServers.size() )
    {
        if ( owner )
            owner->fetchFailed ( this, Type::Version );
        return;
    }

    doFetch ( Type::Version );
}

void MainUpdater::downloadComplete ( HttpDownload *httpDownload )
{
    ASSERT ( _httpDownload.get() == httpDownload );
    ASSERT ( _type == Type::ChangeLog || _type == Type::Archive );

    _httpDownload.reset();

    if ( owner )
        owner->fetchCompleted ( this, _type );
}

void MainUpdater::downloadFailed ( HttpDownload *httpDownload )
{
    ASSERT ( _httpDownload.get() == httpDownload );
    ASSERT ( _type == Type::ChangeLog || _type == Type::Archive );

    _httpDownload.reset();

    ++_currentServerIdx;

    if ( _currentServerIdx >= updateServers.size() )
    {
        if ( owner )
            owner->fetchFailed ( this, _type );
        return;
    }

    doFetch ( _type );

    // Reset the download progress to zero
    downloadProgress ( _httpDownload.get(), 0, 1 );
}

void MainUpdater::downloadProgress ( HttpDownload *httpDownload, uint32_t downloadedBytes, uint32_t totalBytes )
{
    if ( owner )
        owner->fetchProgress ( this, _type, double ( downloadedBytes ) / totalBytes );

    LOG ( "%u / %u", downloadedBytes, totalBytes );
}
