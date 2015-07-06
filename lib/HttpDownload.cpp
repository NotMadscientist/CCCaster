#include "HttpDownload.hpp"
#include "Logger.hpp"

using namespace std;


HttpDownload::HttpDownload ( Owner *owner, const string& url, const string& file )
    : owner ( owner ), url ( url ), file ( file ) {}

void HttpDownload::httpResponse ( HttpGet *httpGet, int code, const string& data, uint32_t remainingBytes )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Received HTTP response (%d): [ %u bytes ]", code, data.size() );

    if ( code != 200 )
    {
        httpFailed ( httpGet );
        return;
    }

    outputFile.write ( &data[0], data.size() );

    if ( remainingBytes > 0 )
        return;

    stop();

    if ( owner )
        owner->downloadComplete ( this );
}

void HttpDownload::httpFailed ( HttpGet *httpGet )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Download failed for: %s", httpGet->url );

    stop();

    if ( owner )
        owner->downloadFailed ( this );
}

void HttpDownload::httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes )
{
    if ( owner )
        owner->downloadProgress ( this, receivedBytes, totalBytes );
}

void HttpDownload::start()
{
    outputFile.open ( file.c_str(), ios::binary );

    httpGet.reset ( new HttpGet ( this, url, DEFAULT_GET_TIMEOUT, HttpGet::Incremental ) );
    httpGet->start();
}

void HttpDownload::stop()
{
    outputFile.close();

    httpGet.reset();
}
