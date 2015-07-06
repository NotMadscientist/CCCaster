#pragma once

#include "HttpGet.hpp"

#include <string>
#include <memory>


class ExternalIpAddress : public HttpGet::Owner
{
public:

    struct Owner
    {
        virtual void foundExternalIpAddress ( ExternalIpAddress *extIpAddr, const std::string& address ) = 0;

        // Note: this sets address to the string "unknown"
        virtual void unknownExternalIpAddress ( ExternalIpAddress *extIpAddr ) = 0;
    };

    Owner *owner = 0;

private:

    std::shared_ptr<HttpGet> httpGet;

    size_t nextQueryIndex = 0;

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void httpFailed ( HttpGet *httpGet ) override;

    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override {}

public:

    static const std::string Unknown;

    std::string address;

    ExternalIpAddress ( Owner *owner );

    void start();

    void stop();
};
