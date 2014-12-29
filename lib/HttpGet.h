#pragma once

#include "Socket.h"
#include "Timer.h"

#include <string>


#define DEFAULT_GET_TIMEOUT ( 5000 )


class HttpGet : public Socket::Owner, public Timer::Owner
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

    void timerExpired ( Timer *timer ) override;

    bool tryParse();

    SocketPtr socket;

    TimerPtr timer;

    std::string host, path;

    int code = -1;

    std::string buffer, data;

public:

    const std::string url;

    const uint64_t timeout;

    HttpGet ( Owner *owner, const std::string& url, uint64_t timeout = DEFAULT_GET_TIMEOUT );

    void start();

    int getCode() const { return code; }

    const std::string& getData() const { return data; }
};
