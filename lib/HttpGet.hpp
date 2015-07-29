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

    int getStatusCode() const { return _statusCode; }

    const std::string& getResponse() const { return _dataBuffer; }

    uint32_t getContentLength() const { return _contentLength; }

private:

    SocketPtr _socket;

    TimerPtr _timer;

    std::string _host, _path;

    int _statusCode;

    std::string _headerBuffer, _dataBuffer;

    uint32_t _contentLength, _remainingBytes;

    void socketAccepted ( Socket *socket ) override {}
    void socketConnected ( Socket *socket ) override;
    void socketDisconnected ( Socket *socket ) override;
    void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override {}
    void socketRead ( Socket *socket, const char *bytes, size_t len, const IpAddrPort& address ) override;

    void timerExpired ( Timer *timer ) override;

    void parseResponse ( const std::string& data );

    void parseData ( const std::string& data );

    void finalize();

};
