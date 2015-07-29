#pragma once

#include "Protocol.hpp"
#include "Algorithms.hpp"

#include <cereal/types/string.hpp>

#include <memory>


struct addrinfo;
struct sockaddr;


// IP address utility functions
std::shared_ptr<addrinfo> getAddrInfo ( const std::string& addr, uint16_t port, bool isV4, bool passive = false );

std::string getAddrFromSockAddr ( const sockaddr *sa );

uint16_t getPortFromSockAddr ( const sockaddr *sa );

const char *inet_ntop ( int af, const void *src, char *dst, size_t size );


// IP address with port
class IpAddrPort : public SerializableSequence
{
public:

    std::string addr;
    uint16_t port = 0;
    uint8_t isV4 = true;

    IpAddrPort ( const char *addr, uint16_t port ) : IpAddrPort ( std::string ( addr ), port ) {}
    IpAddrPort ( const std::string& addr, uint16_t port ) : addr ( addr ), port ( port ) {}

    IpAddrPort ( const char *addrPort ) : IpAddrPort ( std::string ( addrPort ) ) {}
    IpAddrPort ( const std::string& addrPort );

    IpAddrPort ( const sockaddr *sa );

    IpAddrPort& operator= ( const IpAddrPort& other )
    {
        addr = other.addr;
        port = other.port;
        isV4 = other.isV4;
        invalidate();
        return *this;
    }

    void invalidate() const override
    {
        Serializable::invalidate();
        _addrInfo.reset();
    }

    const std::shared_ptr<addrinfo>& getAddrInfo() const;

    bool empty() const
    {
        return ( addr.empty() && !port );
    }

    void clear()
    {
        addr.clear();
        port = 0;
        invalidate();
    }

    std::string str() const override
    {
        if ( empty() )
            return "";
        std::stringstream ss;
        ss << addr << ':' << port;
        return ss.str();
    }

    const char *c_str() const
    {
        if ( empty() )
            return "";
        static char buffer[256];
        std::snprintf ( buffer, sizeof ( buffer ), "%s:%u", addr.c_str(), port );
        return buffer;
    }

    PROTOCOL_MESSAGE_BOILERPLATE ( IpAddrPort, addr, port, isV4 )

private:

    mutable std::shared_ptr<addrinfo> _addrInfo;
};


const IpAddrPort NullAddress;


// Hash function
namespace std
{

template<> struct hash<IpAddrPort>
{
    size_t operator() ( const IpAddrPort& a ) const
    {
        size_t seed = 0;
        hash_combine ( seed, a.addr );
        hash_combine ( seed, a.port );
        return seed;
    }
};

} // namespace std


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
