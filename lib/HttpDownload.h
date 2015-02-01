#pragma once

#include "HttpGet.h"

#include <string>
#include <memory>
#include <fstream>


class HttpDownload : public HttpGet::Owner
{
public:

    struct Owner
    {
        virtual void downloadComplete ( HttpDownload *httpDl ) = 0;

        virtual void downloadFailed ( HttpDownload *httpDl ) = 0;

        virtual void downloadProgress ( HttpDownload *httpDl, uint32_t downloadedBytes, uint32_t totalBytes ) = 0;
    };

    Owner *owner = 0;

private:

    std::ofstream outputFile;

    std::shared_ptr<HttpGet> httpGet;

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void httpFailed ( HttpGet *httpGet ) override;

    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override;

public:

    const std::string url, file;

    HttpDownload ( Owner *owner, const std::string& url, const std::string& file );

    void start();

    void stop();
};
