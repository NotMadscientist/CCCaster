#include "HttpGet.h"
#include "TcpSocket.h"
#include "Exceptions.h"

#include <sstream>

using namespace std;

#define USER_AGENT "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1)"


HttpGet::HttpGet ( Owner *owner, const string& url, uint64_t timeout )
    : owner ( owner ), url ( url ), timeout ( timeout )
{
    if ( url.substr ( 0, 8 ) == "https://" )
        THROW_EXCEPTION ( "url='%s'", "Unsupported https protocol!", url );

    if ( url.substr ( 0, 7 ) == "http://" )
        host = url.substr ( 7 );

    size_t i = host.find_last_of ( '/' );

    if ( i < string::npos - 1 )
        path = host.substr ( i + 1 );

    if ( path.empty() )
        path = "/";

    if ( i != string::npos )
        host = host.substr ( 0, i );

    ASSERT ( host.empty() == false );
    ASSERT ( path.empty() == false );
}

void HttpGet::start()
{
    LOG ( "Connecting to: '%s:80'", host );

    try
    {
        socket = TcpSocket::connect ( this, host + ":80", true ); // Raw socket
    }
    catch ( ... )
    {
        socket.reset();
        disconnectEvent ( 0 );
    }
}

void HttpGet::connectEvent ( Socket *socket )
{
    ASSERT ( this->socket.get() == socket );

    string request = format ( "GET %s HTTP/1.1\r\nUser-Agent: %s\r\nHost: %s\r\n\r\n", path, USER_AGENT, host );

    LOG ( "Sending request:\n%s", request );

    timer.reset ( new Timer ( this ) );
    timer->start ( timeout );

    if ( !socket->send ( &request[0], request.size() ) )
        disconnectEvent ( socket );
}

void HttpGet::disconnectEvent ( Socket *socket )
{
    ASSERT ( this->socket.get() == socket );

    if ( tryParse() )
        return;

    if ( owner )
        owner->failedHttp ( this );
}

void HttpGet::timerExpired ( Timer *timer )
{
    ASSERT ( this->timer.get() == timer );

    if ( tryParse() )
        return;

    if ( owner )
        owner->failedHttp ( this );
}

void HttpGet::readEvent ( Socket *socket, const char *data, size_t len, const IpAddrPort& address )
{
    ASSERT ( this->socket.get() == socket );

    buffer += string ( data, len );

    timer->start ( timeout );

    tryParse();
}

bool HttpGet::tryParse()
{
    LOG ( "Trying to parse response:\n%s", buffer );

    stringstream ss ( buffer );
    string header;
    uint32_t contentLength = 0;

    // Get header and status code
    ss >> header >> code;

    // Skip all the headers
    while ( getline ( ss, header ) )
    {
        LOG ( "Header: '%s'", trimmed ( header ) );

        if ( header.find ( "Content-Length:" ) == 0 )
        {
            stringstream ss ( header );
            ss >> header >> contentLength;

            LOG ( "contentLength=%u", contentLength );

            continue;
        }

        if ( header == "\r" )
            break;
    }

    // Sanity check
    if ( !ss.good() )
        return false;

    // Get remaining bytes
    size_t remaining = ss.rdbuf()->in_avail();

    if ( remaining < contentLength )
        return false;

    ASSERT ( buffer.size() >= remaining );

    this->data.resize ( remaining );
    ss.rdbuf()->sgetn ( &this->data[0], remaining );

    socket.reset();
    timer.reset();

    if ( owner )
        owner->receivedHttp ( this, code, this->data );
    return true;
}
