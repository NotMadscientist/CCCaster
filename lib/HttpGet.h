#pragma once

#include "Socket.h"

#include <string>


class HttpGet : public Socket::Owner
{
public:

    struct Owner
    {
        virtual void receivedHttp ( HttpGet *httpGet, int code, const std::string& data ) = 0;

        virtual void failedHttp ( HttpGet *httpGet ) = 0;
    };

    Owner *owner = 0;

private:

    void acceptEvent ( Socket *socket ) override {}
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {}
    void readEvent ( Socket *socket, const char *data, size_t len, const IpAddrPort& address ) override;

    SocketPtr socket;

    std::string host, path;

    int code = -1;

    std::string data;

public:

    const std::string url;

    HttpGet ( Owner *owner, const std::string& url );

    void start();

    int getCode() const { return code; }

    const std::string& getData() const { return data; }
};
