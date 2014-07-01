#pragma once

#include "Protocol.h"

#include <cereal/types/string.hpp>

#include <memory>

struct addrinfo;

struct IpAddrPort : public SerializableMessage
{
    std::string addr;
    unsigned port;

    mutable std::shared_ptr<addrinfo> addrInfo;

    inline IpAddrPort() : port ( 0 ) {}

    inline IpAddrPort ( const std::string& addr, unsigned port )
        : addr ( addr ), port ( port ) {}

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

    std::shared_ptr<addrinfo>& updateAddrInfo ( bool passive = false ) const;

    MsgType getMsgType() const override;

protected:

    void serialize ( cereal::BinaryOutputArchive& ar ) const override { ar ( addr, port ); }

    void deserialize ( cereal::BinaryInputArchive& ar ) override { ar ( addr, port ); }
};

const IpAddrPort NullAddress;

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
