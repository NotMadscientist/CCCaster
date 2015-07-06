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

    const Version& getLatestVersion() const { return latestVersion; }

private:

    Type type;

    std::shared_ptr<HttpGet> httpGet;

    std::shared_ptr<HttpDownload> httpDownload;

    uint32_t currentServerIdx = 0;

    Version latestVersion;

    std::string downloadDir;

    void doFetch ( const Type& type );

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;
    void httpFailed ( HttpGet *httpGet ) override;
    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override {}

    void downloadComplete ( HttpDownload *httpDownload ) override;
    void downloadFailed ( HttpDownload *httpDownload ) override;
    void downloadProgress ( HttpDownload *httpDownload, uint32_t downloadedBytes, uint32_t totalBytes ) override;
};
