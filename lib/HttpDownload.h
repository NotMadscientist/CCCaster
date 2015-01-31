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
    };

    Owner *owner = 0;

private:

    std::ofstream outputFile;

    std::shared_ptr<HttpGet> httpGet;

    void receivedHttp ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void failedHttp ( HttpGet *httpGet ) override;

public:

    const std::string url, file;

    HttpDownload ( Owner *owner, const std::string& url, const std::string& file );

    void start();

    void stop();
};
