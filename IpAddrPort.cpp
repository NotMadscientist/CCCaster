#include "IpAddrPort.h"
#include "Log.h"
#include "Util.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

using namespace std;

shared_ptr<addrinfo>& IpAddrPort::updateAddrInfo ( bool passive ) const
{
    addrinfo addrConf, *addrRes = 0;
    ZeroMemory ( &addrConf, sizeof ( addrConf ) );

    addrConf.ai_family = AF_INET; // TODO IPv6

    if ( passive )
        addrConf.ai_flags = AI_PASSIVE;

    int error = getaddrinfo ( addr.empty() ? 0 : addr.c_str(), TO_C_STR ( port ), &addrConf, &addrRes );

    if ( error != 0 )
    {
        LOG ( "getaddrinfo failed: %s", getWindowsErrorAsString ( error ).c_str() );
        throw "something"; // TODO
    }

    addrInfo.reset ( addrRes, freeaddrinfo );
    return addrInfo;
}
