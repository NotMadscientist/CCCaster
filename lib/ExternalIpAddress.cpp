#include "ExternalIpAddress.hpp"
#include "Logger.hpp"

#include <vector>

using namespace std;


// Unknown IP address
const string ExternalIpAddress::Unknown = "Unknown";

// Web services to query for external IP address
static const vector<string> ExternalIpServices =
{
    "http://checkip.amazonaws.com",
    "http://ipv4.wtfismyip.com/text",
    "http://ipv4.icanhazip.com",
    "http://ifcfg.net",
};


ExternalIpAddress::ExternalIpAddress ( Owner *owner ) : owner ( owner ) {}

void ExternalIpAddress::httpResponse ( HttpGet *httpGet, int code, const string& data, uint32_t remainingBytes )
{
    ASSERT ( _httpGet.get() == httpGet );

    LOG ( "Received HTTP response (%d): '%s'", code, data );

    if ( code != 200 || data.size() < 7 ) // Min IPv4 length, eg "1.1.1.1" TODO actually validate this
    {
        httpFailed ( httpGet );
        return;
    }

    address = trimmed ( data );

    _httpGet.reset();

    if ( owner )
        owner->externalIpAddrFound ( this, address );
}

void ExternalIpAddress::httpFailed ( HttpGet *httpGet )
{
    ASSERT ( _httpGet.get() == httpGet );

    LOG ( "HTTP GET failed for: %s", _httpGet->url );

    _httpGet.reset();

    if ( _nextQueryIndex == ExternalIpServices.size() )
    {
        address = Unknown;

        if ( owner )
            owner->externalIpAddrUnknown ( this );
        return;
    }

    _httpGet.reset ( new HttpGet ( this, ExternalIpServices[_nextQueryIndex++] ) );
    _httpGet->start();
}

void ExternalIpAddress::start()
{
    address.clear();

    _nextQueryIndex = 1;

    _httpGet.reset ( new HttpGet ( this, ExternalIpServices[0] ) );
    _httpGet->start();
}

void ExternalIpAddress::stop()
{
    _httpGet.reset();
}
