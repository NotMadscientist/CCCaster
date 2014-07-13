#pragma once

#include "Protocol.h"
#include "Utilities.h"

#include <cereal/types/string.hpp>

#include <memory>
#include <algorithm>

struct addrinfo;
struct sockaddr;

// IP address utility functions
std::shared_ptr<addrinfo> getAddrInfo ( const std::string& addr, uint16_t port, bool isV4, bool passive = false );
std::string getAddrFromSockAddr ( const sockaddr *sa );
uint16_t getPortFromSockAddr ( const sockaddr *sa );
const char *inet_ntop ( int af, const void *src, char *dst, size_t size );

// IP address with port
struct IpAddrPort : public SerializableMessage
{
    std::string addr;
    uint16_t port;
    bool isV4;

    inline explicit IpAddrPort ( bool isV4 = true )
        : port ( 0 ), isV4 ( isV4 ) {}

    inline IpAddrPort ( const std::string& addr, uint16_t port, bool isV4 = true )
        : addr ( addr ), port ( port ), isV4 ( isV4 ) {}

    IpAddrPort ( const sockaddr *sa );

    const std::shared_ptr<addrinfo>& getAddrInfo() const;

    inline bool empty() const
    {
        return ( addr.empty() && !port );
    }

    inline void clear()
    {
        addr.clear();
        port = 0;
    }

    inline std::string str() const
    {
        if ( empty() )
            return "";
        std::stringstream ss;
        ss << addr << ':' << port;
        return ss.str();
    }

    inline const char *c_str() const
    {
        if ( empty() )
            return "";
        static char buffer[256];
        std::sprintf ( buffer, "%s:%u", addr.c_str(), port );
        return buffer;
    }

    PROTOCOL_BOILERPLATE ( addr, port )

private:

    mutable std::shared_ptr<addrinfo> addrInfo;
};

const IpAddrPort NullAddress;

// Hash function
namespace std
{

template<> struct hash<IpAddrPort>
{
    inline size_t operator() ( const IpAddrPort& a ) const
    {
        size_t seed = 0;
        hash_combine ( seed, a.addr );
        hash_combine ( seed, a.port );
        return seed;
    }
};

}

// Comparison operators
inline bool operator< ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ( a.addr < b.addr && a.port < b.port );
}

inline bool operator== ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ( a.addr == b.addr && a.port == b.port );
}

inline bool operator!= ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ! ( a == b );
}

// Stream operator
inline std::ostream& operator<< ( std::ostream& os, const IpAddrPort& a ) { return ( os << a.str() ); }
