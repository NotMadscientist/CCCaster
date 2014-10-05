#pragma once

#include "HttpGet.h"

#include <string>
#include <memory>


class ExternalIpAddress : public HttpGet::Owner
{
public:

    struct Owner
    {
        virtual void foundExternalIpAddress ( ExternalIpAddress *extIpAddr, const std::string& address ) {}

        // Note: this sets address to "unknown"
        virtual void unknownExternalIpAddress ( ExternalIpAddress *extIpAddr ) {};
    };

    Owner *owner = 0;

private:

    std::shared_ptr<HttpGet> httpGet;

    size_t nextQueryIndex = 0;

    void receivedHttp ( HttpGet *httpGet, int code, const std::string& data ) override;

    void failedHttp ( HttpGet *httpGet ) override;

public:

    std::string address;

    ExternalIpAddress ( Owner *owner );

    void start();
};
