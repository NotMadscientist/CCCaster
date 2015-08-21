#pragma once

#include "Enum.hpp"
#include "HttpDownload.hpp"
#include "HttpGet.hpp"
#include "Version.hpp"

#include <string>


class MainUpdater
    : private HttpDownload::Owner
    , private HttpGet::Owner
{
public:

    ENUM ( Type, Version, ChangeLog, Archive );

    struct Owner
    {
        virtual void fetchCompleted ( MainUpdater *updater, const Type& type ) = 0;

        virtual void fetchFailed ( MainUpdater *updater, const Type& type ) = 0;

        virtual void fetchProgress ( MainUpdater *updater, const Type& type, double progress ) = 0;
    };

    Owner *owner = 0;

    MainUpdater ( Owner *owner );

    void fetch ( const Type& type );

    bool openChangeLog() const;

    bool extractArchive() const;

    Type getType() const { return _type; }

    const Version& getLatestVersion() const { return _latestVersion; }

private:

    Type _type;

    std::shared_ptr<HttpGet> _httpGet;

    std::shared_ptr<HttpDownload> _httpDownload;

    uint32_t _currentServerIdx = 0;

    Version _latestVersion;

    std::string _downloadDir;

    void doFetch ( const Type& type );

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;
    void httpFailed ( HttpGet *httpGet ) override;
    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override {}

    void downloadComplete ( HttpDownload *httpDownload ) override;
    void downloadFailed ( HttpDownload *httpDownload ) override;
    void downloadProgress ( HttpDownload *httpDownload, uint32_t downloadedBytes, uint32_t totalBytes ) override;
};
