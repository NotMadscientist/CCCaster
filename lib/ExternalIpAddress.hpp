#pragma once

#include "HttpGet.hpp"

#include <string>
#include <memory>


class ExternalIpAddress : private HttpGet::Owner
{
public:

    struct Owner
    {
        virtual void externalIpAddrFound ( ExternalIpAddress *extIpAddr, const std::string& address ) = 0;

        // Note: the address will be set to the string Unknown
        virtual void externalIpAddrUnknown ( ExternalIpAddress *extIpAddr ) = 0;
    };

    Owner *owner = 0;

    std::string address;

    ExternalIpAddress ( Owner *owner );

    void start();

    void stop();

    static const std::string Unknown;

private:

    std::shared_ptr<HttpGet> _httpGet;

    size_t _nextQueryIndex = 0;

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void httpFailed ( HttpGet *httpGet ) override;

    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override {}
};
