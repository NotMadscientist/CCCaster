#pragma once

#include "Socket.hpp"
#include "Timer.hpp"

#include <string>


#define DEFAULT_GET_TIMEOUT ( 5000 )


class HttpGet
    : private Socket::Owner
    , private Timer::Owner
{
public:

    struct Owner
    {
        virtual void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) = 0;

        virtual void httpFailed ( HttpGet *httpGet ) = 0;

        virtual void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) = 0;
    };

    Owner *owner = 0;

    const std::string url;

    const uint64_t timeout;

    const enum Mode { Buffered, Incremental } mode;

    HttpGet ( Owner *owner, const std::string& url, uint64_t timeout = DEFAULT_GET_TIMEOUT, Mode mode = Buffered );

    void start();

    int getCode() const { return code; }

    const std::string& getResponse() const { return dataBuffer; }

    uint32_t getContentLength() const { return contentLength; }

private:

    void socketAccepted ( Socket *socket ) override {}
    void socketConnected ( Socket *socket ) override;
    void socketDisconnected ( Socket *socket ) override;
    void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {}
    void socketRead ( Socket *socket, const char *bytes, size_t len, const IpAddrPort& address ) override;

    void timerExpired ( Timer *timer ) override;

    void parseResponse ( const std::string& data );

    void parseData ( const std::string& data );

    void finalize();

    SocketPtr socket;

    TimerPtr timer;

    std::string host, path;

    int code;

    std::string headerBuffer, dataBuffer;

    uint32_t contentLength, remainingBytes;
};
