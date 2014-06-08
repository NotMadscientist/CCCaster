#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>
#include <netlink/socket.h>

struct IpAddrPort : public Serializable
{
    std::string addr;
    unsigned port;

    inline IpAddrPort() : port ( 0 ) {}

    inline IpAddrPort ( const std::shared_ptr<NL::Socket>& socket )
        : addr ( socket->hostTo() ), port ( socket->portTo() ) {}

    inline IpAddrPort ( const NL::Socket *socket )
        : addr ( socket->hostTo() ), port ( socket->portTo() ) {}

    inline IpAddrPort ( const std::string& addr, unsigned port )
        : addr ( addr ), port ( port ) {}

    inline bool empty() const
    {
        return ( addr.empty() || !port );
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

    void serialize ( cereal::BinaryOutputArchive& ar ) const { ar ( addr, port ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) { ar ( addr, port ); }

    uint32_t type() const;
};

inline bool operator== ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ( a.addr == b.addr && a.port == b.port );
}

inline bool operator!= ( const IpAddrPort& a, const IpAddrPort& b )
{
    return ! ( a == b );
}

namespace std
{

template<class T>
inline void hash_combine ( size_t& seed, const T& v )
{
    hash<T> hasher;
    seed ^= hasher ( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

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
