#include "HttpGet.hpp"
#include "TcpSocket.hpp"
#include "Exceptions.hpp"

#include <sstream>

using namespace std;

#define USER_AGENT "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1)"


HttpGet::HttpGet ( Owner *owner, const string& url, uint64_t timeout, Mode mode )
    : owner ( owner )
    , url ( url )
    , timeout ( timeout )
    , mode ( mode )
{
    if ( url.substr ( 0, 8 ) == "https://" )
        THROW_EXCEPTION ( "url='%s'", "Unsupported https protocol!", url );

    if ( url.substr ( 0, 7 ) == "http://" )
        _host = url.substr ( 7 );

    const size_t i = _host.find_last_of ( '/' );

    if ( i < string::npos - 1 )
        _path = _host.substr ( i );

    if ( _path.empty() )
        _path = "/";

    if ( i != string::npos )
        _host = _host.substr ( 0, i );

    ASSERT ( _host.empty() == false );
    ASSERT ( _path.empty() == false );
}

void HttpGet::start()
{
    _statusCode = -1;
    _headerBuffer.clear();
    _dataBuffer.clear();
    _remainingBytes = 0;

    LOG ( "Connecting to: '%s:80'", _host );

    try
    {
        _socket = TcpSocket::connect ( this, _host + ":80", true, timeout ); // Raw socket
    }
    catch ( ... )
    {
        _socket.reset();

        LOG ( "Failed to create socket!" );

        if ( owner )
            owner->httpFailed ( this );
    }
}

void HttpGet::socketConnected ( Socket *socket )
{
    ASSERT ( _socket.get() == socket );

    const string request = format ( "GET %s HTTP/1.1\r\nUser-Agent: %s\r\nHost: %s\r\n\r\n", _path, USER_AGENT, _host );

    LOG ( "Sending request:\n%s", request );

    _timer.reset ( new Timer ( this ) );
    _timer->start ( timeout );

    if ( ! _socket->send ( &request[0], request.size() ) )
    {
        LOG ( "Failed to send request!" );

        if ( owner )
            owner->httpFailed ( this );
    }
}

void HttpGet::socketDisconnected ( Socket *socket )
{
    ASSERT ( _socket.get() == socket );

    if ( _statusCode >= 0 && _remainingBytes == 0 )
        return;

    _socket.reset();
    _timer.reset();

    if ( owner )
        owner->httpFailed ( this );
}

void HttpGet::timerExpired ( Timer *timer )
{
    ASSERT ( _timer.get() == timer );

    if ( _statusCode >= 0 && _remainingBytes == 0 )
        return;

    _socket.reset();
    _timer.reset();

    if ( owner )
        owner->httpFailed ( this );
}

void HttpGet::socketRead ( Socket *socket, const char *bytes, size_t len, const IpAddrPort& address )
{
    ASSERT ( _socket.get() == socket );

    const string data ( bytes, len );

    if ( _remainingBytes == 0 )
        parseResponse ( data );
    else
        parseData ( data );
}

void HttpGet::parseResponse ( const string& data )
{
    // Append to the read buffer
    _headerBuffer += data;

    // Start a timeout for the next read event
    _timer->start ( timeout );

    LOG ( "Trying to parse response:\n%s", _headerBuffer );

    stringstream ss ( _headerBuffer );
    string header;

    // Get the HTTP version header and status code
    ss >> header >> _statusCode;

    while ( getline ( ss, header ) )
    {
        // Stop if we reached the end of the headers
        if ( header == "\r" )
        {
            _remainingBytes = _contentLength;

            // Get the remaining response bytes from the header buffer
            const size_t responseBytes = ss.rdbuf()->in_avail();

            if ( responseBytes == 0 )
            {
                finalize();
                return;
            }

            string data ( responseBytes, '\0' );
            ss.rdbuf()->sgetn ( &data[0], responseBytes );

            parseData ( data );
            return;
        }

        LOG ( "Header: '%s'", trimmed ( header ) );

        // Skip all the headers except Content-Length
        if ( header.find ( "Content-Length:" ) == 0 )
        {
            stringstream ss ( header );
            ss >> header >> _contentLength;

            LOG ( "contentLength=%u", _contentLength );
            continue;
        }
    }
}

void HttpGet::parseData ( const string& data )
{
    _remainingBytes -= data.size();

    if ( owner && _contentLength )
        owner->httpProgress ( this, _contentLength - _remainingBytes, _contentLength );

    if ( mode == Buffered )
    {
        _dataBuffer += data;
    }
    else if ( mode == Incremental )
    {
        if ( _remainingBytes == 0 )
        {
            _socket.reset();
            _timer.reset();
        }

        if ( owner )
            owner->httpResponse ( this, _statusCode, data, _remainingBytes );
        return;
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }

    if ( _remainingBytes == 0 )
        finalize();
}

void HttpGet::finalize()
{
    ASSERT ( _remainingBytes == 0 );

    _socket.reset();
    _timer.reset();

    if ( owner )
        owner->httpResponse ( this, _statusCode, _dataBuffer, 0 );
}
