#include "HttpDownload.h"
#include "Logger.h"

using namespace std;


HttpDownload::HttpDownload ( Owner *owner, const string& url, const string& file )
    : owner ( owner ), url ( url ), file ( file ) {}

void HttpDownload::receivedHttp ( HttpGet *httpGet, int code, const string& data, uint32_t remainingBytes )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Received HTTP response (%d): [ %u bytes ]", code, data.size() );

    outputFile.write ( &data[0], data.size() );

    if ( remainingBytes > 0 )
        return;

    stop();

    if ( owner )
        owner->downloadComplete ( this );
}

void HttpDownload::failedHttp ( HttpGet *httpGet )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Download failed for: %s", httpGet->url );

    if ( owner )
        owner->downloadFailed ( this );
}

void HttpDownload::start()
{
    outputFile.open ( file.c_str(), ios::binary );

    httpGet.reset ( new HttpGet ( this, url ) );
    httpGet->start();
}

void HttpDownload::stop()
{
    outputFile.close();

    httpGet.reset();
}
