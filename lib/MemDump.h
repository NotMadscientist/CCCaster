#pragma once

#include <vector>
#include <memory>


struct MemList;


struct MemDump
{
    char *const addr; // or offset if child
    const size_t size;

    size_t totalSize;
    std::shared_ptr<MemList> children;

    template<typename T>
    MemDump ( T *addr )
        : addr ( ( char * ) addr ), size ( sizeof ( *addr ) ), totalSize ( sizeof ( *addr ) ) {}

    MemDump ( void *addr, size_t size )
        : addr ( ( char * ) addr ), size ( size ), totalSize ( size ) {}

    MemDump ( void *addr, const MemDump& childAddr );

    MemDump copy ( size_t plusAddr = 0, size_t plusSize = 0 ) const;

    static MemDump child ( size_t offset, size_t size )
    {
        return MemDump ( ChildAddress, offset, size );
    }

    static MemDump child ( size_t offset, const MemDump& childAddr )
    {
        return MemDump ( ChildAddress, offset, childAddr );
    }

private:

    enum ChildAddressEnum { ChildAddress };

    MemDump ( ChildAddressEnum, size_t offset, size_t size )
        : addr ( ( char * ) offset ), size ( size ), totalSize ( size ) {}

    MemDump ( ChildAddressEnum, size_t offset, const MemDump& childAddr );

    MemDump ( char *addr, size_t size, size_t totalSize )
        : addr (  addr ), size ( size ), totalSize ( totalSize ) {}

    MemDump ( char *addr, size_t size, size_t totalSize, const MemList& children );
};


struct MemList
{
    std::vector<MemDump> addresses;
    size_t totalSize = 0;

    MemList() {}

    MemList ( const MemDump& addr ) : totalSize ( addr.totalSize ) { addresses.push_back ( addr.copy() ); }

    MemList ( const MemList& addresses ) { append ( addresses.addresses ); optimize(); }

    MemList ( const std::vector<MemDump>& addresses ) { append ( addresses ); optimize(); }

    void append ( const MemList& other, size_t addrOffset = 0 ) { append ( other.addresses, addrOffset ); }

    void append ( const std::vector<MemDump>& other, size_t addrOffset = 0 )
    {
        for ( const MemDump& addr : other )
            addresses.push_back ( addr.copy ( addrOffset ) );
    }

    void optimize();
};


inline MemDump::MemDump ( void *addr, const MemDump& childAddr )
    : addr ( ( char * ) addr ), size ( 4 ), totalSize ( 4 + childAddr.totalSize )
    , children ( new MemList ( childAddr ) ) {}

inline MemDump::MemDump ( ChildAddressEnum, size_t offset, const MemDump& childAddr )
    : addr ( ( char * ) offset ), size ( 4 ), totalSize ( 4 + childAddr.totalSize )
    , children ( new MemList ( childAddr ) ) {}

inline MemDump::MemDump ( char *addr, size_t size, size_t totalSize, const MemList& children )
    : addr ( addr ), size ( size ), totalSize ( totalSize )
    , children ( new MemList ( children ) ) {}

inline MemDump MemDump::copy ( size_t plusAddr, size_t plusSize ) const
{
    if ( children )
        return MemDump ( addr + plusAddr, size + plusSize, totalSize + plusSize, *children );
    else
        return MemDump ( addr + plusAddr, size + plusSize, totalSize + plusSize );
}
