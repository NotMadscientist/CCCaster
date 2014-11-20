#include "ExternalIpAddress.h"
#include "Logger.h"

#include <vector>

using namespace std;


// Web services to query for external IP address
static vector<string> externalIpServices =
{
    "http://checkip.amazonaws.com",
    "http://ifcfg.net",
    "http://ipv4.icanhazip.com",
    "http://ifconfig.me/ip"
};

ExternalIpAddress::ExternalIpAddress ( Owner *owner ) : owner ( owner ) {}

void ExternalIpAddress::receivedHttp ( HttpGet *httpGet, int code, const string& data )
{
    ASSERT ( this->httpGet.get() == httpGet );

    LOG ( "Received HTTP response (%d): '%s'", code, data );

    if ( code != 200 )
    {
        failedHttp ( httpGet );
        return;
    }

    address = trim ( data );
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
