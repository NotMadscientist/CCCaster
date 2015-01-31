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
        virtual void receivedHttp ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) = 0;

        virtual void failedHttp ( HttpGet *httpGet ) = 0;
    };

    Owner *owner = 0;

private:

    void acceptEvent ( Socket *socket ) override {}
    void connectEvent ( Socket *socket ) override;
    void disconnectEvent ( Socket *socket ) override;
    void readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {}
    void readEvent ( Socket *socket, const char *bytes, size_t len, const IpAddrPort& address ) override;

    void timerExpired ( Timer *timer ) override;

    void parseResponse ( const std::string& data );

    void parseData ( const std::string& data );

    void finalize();

    SocketPtr socket;

    TimerPtr timer;

    std::string host, path;

    int code;

    std::string headerBuffer, dataBuffer;

    uint32_t remainingBytes;

public:

    const std::string url;

    const uint64_t timeout;

    const enum Mode { Buffered, Incremental } mode;

    HttpGet ( Owner *owner, const std::string& url, uint64_t timeout = DEFAULT_GET_TIMEOUT, Mode mode = Buffered );

    void start();

    int getCode() const { return code; }

    const std::string& getResponse() const { return dataBuffer; }
};
