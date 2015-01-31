#include "ExternalIpAddress.h"
#include "Logger.h"

#include <vector>

using namespace std;


// Unknown IP address
const string ExternalIpAddress::Unknown = "unknown";

// Web services to query for external IP address
static const vector<string> externalIpServices =
{
    "http://checkip.amazonaws.com",
    "http://ifcfg.net",
    "http://ipv4.icanhazip.com",
};

ExternalIpAddress::ExternalIpAddress ( Owner *owner ) : owner ( owner ) {}

void ExternalIpAddress::receivedHttp ( HttpGet *httpGet, int code, const string& data, uint32_t remainingBytes )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Received HTTP response (%d): '%s'", code, data );

    if ( code != 200 || data.size() < 7 ) // Min IPv4 length, eg "1.1.1.1" TODO actually validate this
    {
        failedHttp ( httpGet );
        return;
    }

    address = trimmed ( data );
    this->httpGet.reset();

    if ( owner )
        owner->foundExternalIpAddress ( this, address );
}

void ExternalIpAddress::failedHttp ( HttpGet *httpGet )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "HTTP GET failed for: %s", httpGet->url );

    this->httpGet.reset();

    if ( nextQueryIndex == externalIpServices.size() )
    {
        address = "unknown";

        if ( owner )
            owner->unknownExternalIpAddress ( this );
        return;
    }

    this->httpGet.reset ( new HttpGet ( this, externalIpServices[nextQueryIndex++] ) );
    this->httpGet->start();
}

void ExternalIpAddress::start()
{
    nextQueryIndex = 1;
    address.clear();

    httpGet.reset ( new HttpGet ( this, externalIpServices[0] ) );
    httpGet->start();
}

void ExternalIpAddress::stop()
{
    httpGet.reset();
}
