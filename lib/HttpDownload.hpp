#pragma once

#include "HttpGet.hpp"

#include <string>
#include <memory>
#include <fstream>


class HttpDownload : private HttpGet::Owner
{
public:

    struct Owner
    {
        virtual void downloadComplete ( HttpDownload *httpDownload ) = 0;

        virtual void downloadFailed ( HttpDownload *httpDownload ) = 0;

        virtual void downloadProgress ( HttpDownload *httpDownload, uint32_t downloadedBytes, uint32_t totalBytes ) = 0;
    };

    Owner *owner = 0;

    const std::string url, file;

    HttpDownload ( Owner *owner, const std::string& url, const std::string& file );

    void start();

    void stop();

private:

    std::ofstream _outputFile;

    std::shared_ptr<HttpGet> _httpGet;

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void httpFailed ( HttpGet *httpGet ) override;

    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override;
};
