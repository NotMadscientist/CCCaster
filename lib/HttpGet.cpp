#include "HttpGet.hpp"
#include "TcpSocket.hpp"
#include "Exceptions.hpp"

#include <sstream>

using namespace std;

#define USER_AGENT "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1)"


HttpGet::HttpGet ( Owner *owner, const string& url, uint64_t timeout, Mode mode )
    : owner ( owner ), url ( url ), timeout ( timeout ), mode ( mode )
{
    if ( url.substr ( 0, 8 ) == "https://" )
        THROW_EXCEPTION ( "url='%s'", "Unsupported https protocol!", url );

    if ( url.substr ( 0, 7 ) == "http://" )
        host = url.substr ( 7 );

    size_t i = host.find_last_of ( '/' );

    if ( i < string::npos - 1 )
        path = host.substr ( i );

    if ( path.empty() )
        path = "/";

    if ( i != string::npos )
        host = host.substr ( 0, i );

    ASSERT ( host.empty() == false );
    ASSERT ( path.empty() == false );
}

void HttpGet::start()
{
    code = -1;
    headerBuffer.clear();
    dataBuffer.clear();
    remainingBytes = 0;

    LOG ( "Connecting to: '%s:80'", host );

    try
    {
        socket = TcpSocket::connect ( this, host + ":80", true, timeout ); // Raw socket
    }
    catch ( ... )
    {
        socket.reset();

        LOG ( "Failed to create socket!" );

        if ( owner )
            owner->httpFailed ( this );
    }
}

void HttpGet::connectEvent ( Socket *socket )
{
    ASSERT ( this->socket.get() == socket );

    string request = format ( "GET %s HTTP/1.1\r\nUser-Agent: %s\r\nHost: %s\r\n\r\n", path, USER_AGENT, host );

    LOG ( "Sending request:\n%s", request );

    timer.reset ( new Timer ( this ) );
    timer->start ( timeout );

    if ( ! socket->send ( &request[0], request.size() ) )
    {
        LOG ( "Failed to send request!" );

        if ( owner )
            owner->httpFailed ( this );
    }
}

void HttpGet::disconnectEvent ( Socket *socket )
{
    ASSERT ( this->socket.get() == socket );

    if ( code >= 0 && remainingBytes == 0 )
        return;

    this->socket.reset();
    this->timer.reset();

    if ( owner )
        owner->httpFailed ( this );
}

void HttpGet::timerExpired ( Timer *timer )
{
    ASSERT ( this->timer.get() == timer );

    if ( code >= 0 && remainingBytes == 0 )
        return;

    this->socket.reset();
    this->timer.reset();

    if ( owner )
        owner->httpFailed ( this );
}

void HttpGet::readEvent ( Socket *socket, const char *bytes, size_t len, const IpAddrPort& address )
{
    ASSERT ( this->socket.get() == socket );

    const string data ( bytes, len );

    if ( remainingBytes == 0 )
        parseResponse ( data );
    else
        parseData ( data );
}

void HttpGet::parseResponse ( const string& data )
{
    // Append to the read buffer
    headerBuffer += data;

    // Start a timeout for the next read event
    timer->start ( timeout );

    LOG ( "Trying to parse response:\n%s", headerBuffer );

    stringstream ss ( headerBuffer );
    string header;

    // Get the HTTP version header and status code
    ss >> header >> code;

    while ( getline ( ss, header ) )
    {
        // Stop if we reached the end of the headers
        if ( header == "\r" )
        {
            remainingBytes = contentLength;

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
            ss >> header >> contentLength;

            LOG ( "contentLength=%u", contentLength );
            continue;
        }
    }
}

void HttpGet::parseData ( const string& data )
{
    remainingBytes -= data.size();

    if ( owner && contentLength )
        owner->httpProgress ( this, contentLength - remainingBytes, contentLength );

    if ( mode == Buffered )
    {
        dataBuffer += data;
    }
    else if ( mode == Incremental )
    {
        if ( remainingBytes == 0 )
        {
            socket.reset();
            timer.reset();
        }

        if ( owner )
            owner->httpResponse ( this, code, data, remainingBytes );
        return;
    }
    else
    {
        ASSERT_IMPOSSIBLE;
    }

    if ( remainingBytes == 0 )
        finalize();
}

void HttpGet::finalize()
{
    ASSERT ( remainingBytes == 0 );

    socket.reset();
    timer.reset();

    if ( owner )
        owner->httpResponse ( this, code, dataBuffer, 0 );
}
