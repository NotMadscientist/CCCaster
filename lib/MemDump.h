#pragma once

#include <cassert>
#include <vector>
#include <memory>


struct MemDumpPtr;


struct MemDumpBase
{
    // Size of this memory dump
    const size_t size;

    // Any child pointers located in this memory dump
    const std::vector<MemDumpPtr> ptrs;

    // Construct a memory dump with the given size
    MemDumpBase ( size_t size ) : size ( size ) {}

    // Construct a memory dump with the given size and child pointers
    MemDumpBase ( size_t size, const std::vector<MemDumpPtr>& ptrs );

    // Get the starting address of this memory dump
    virtual char *getAddr() const = 0;

    // Get the total size of this memory dump
    size_t getTotalSize() const;

protected:

    static std::vector<MemDumpPtr> setParents ( const std::vector<MemDumpPtr>& ptrs, const MemDumpBase *parent );
    static std::vector<MemDumpPtr> addOffsets ( const std::vector<MemDumpPtr>& ptrs, size_t addSrcOffset );
    static std::vector<MemDumpPtr> concat ( const std::vector<MemDumpPtr>& a, const std::vector<MemDumpPtr>& b );
};


struct MemDumpPtr : public MemDumpBase
{
    // The parent memory dump
    const MemDumpBase *const parent = 0;

    // The location of the pointer's value starting from the parent's address
    const size_t srcOffset;

    // The offset to add to the pointer's value
    const size_t dstOffset;

    // Construct a pointer to memory
    MemDumpPtr ( size_t srcOffset, size_t dstOffset, size_t size )
        : MemDumpBase ( size ), srcOffset ( srcOffset ), dstOffset ( dstOffset ) {}

    // Construct a pointer to memory with child pointers
    MemDumpPtr ( size_t srcOffset, size_t dstOffset, size_t size, const std::vector<MemDumpPtr>& ptrs )
        : MemDumpBase ( size, ptrs ), srcOffset ( srcOffset ), dstOffset ( dstOffset ) {}

    // Copy constructor
    MemDumpPtr ( const MemDumpPtr& a )
        : MemDumpBase ( a.size, a.ptrs ), parent ( a.parent ), srcOffset ( a.srcOffset ), dstOffset ( a.dstOffset ) {}

    // Get the starting address of this memory dump
    char *getAddr() const override
    {
        assert ( parent != 0 );
        assert ( srcOffset + 4 <= parent->size );
        return ( * ( char ** ) ( parent->getAddr() + srcOffset ) ) + dstOffset;
    }

private:

    MemDumpPtr ( const MemDumpBase *parent, const std::vector<MemDumpPtr>& ptrs, size_t src, size_t dst, size_t size )
        : MemDumpBase ( size, ptrs ), parent ( parent ), srcOffset ( src ), dstOffset ( dst ) {}

    friend struct MemDumpBase;
};


inline MemDumpBase::MemDumpBase ( size_t size, const std::vector<MemDumpPtr>& ptrs )
    : size ( size ), ptrs ( setParents ( ptrs, this ) ) {}

inline size_t MemDumpBase::getTotalSize() const
{
    size_t totalSize = size;
    for ( const MemDumpPtr& ptr : ptrs )
        totalSize += ptr.getTotalSize();
    return totalSize;
}


struct MemDump : public MemDumpBase
{
    // The starting address of the memory dump
    char *const addr;

    // Construct a memory dump with the pointer type
    template<typename T>
    MemDump ( T *addr )
        : MemDumpBase ( sizeof ( *addr ) ), addr ( ( char * ) addr ) {}

    // Construct a memory dump with the pointer and size
    MemDump ( void *addr, size_t size )
        : MemDumpBase ( size ), addr ( ( char * ) addr ) {}

    // Construct a memory dump with the pointer and size, with child pointers
    MemDump ( void *addr, size_t size, const std::vector<MemDumpPtr>& ptrs )
        : MemDumpBase ( size, ptrs ), addr ( ( char * ) addr ) {}

    // Copy constructor
    MemDump ( const MemDump& a )
        : MemDumpBase ( a.size, a.ptrs ), addr ( a.addr ) {}

    // Merge constructor
    MemDump ( const MemDump& a, const MemDump& b )
        : MemDumpBase ( a.size + b.size, concat ( a.ptrs, addOffsets ( b.ptrs, a.size ) ) ), addr ( a.addr )
    {
        assert ( a.addr + a.size == b.addr );
    }

    // Get the starting address of this memory dump
    char *getAddr() const override { return addr; }
};


struct MemDumpList
{
    // List of memory dumps
    std::vector<MemDump> addrs;

    // Total size of memory dumps, only valid after calling update()
    size_t totalSize = 0;

    // True only if addrs.empty()
    bool empty() { return addrs.empty(); }

    // Append a single memory dump
    void append ( const MemDump& mem )
    {
        addrs.push_back ( mem );
    }

    // Append a single memory dump with address offset
    void append ( const MemDump& mem, size_t addAddrOffset )
    {
        addrs.push_back ( MemDump ( mem.addr + addAddrOffset, mem.size, mem.ptrs ) );
    }

    // Append a list of memory dumps
    void append ( const std::vector<MemDump>& list )
    {
        for ( const MemDump& addr : list )
            append ( addr );
    }

    // Append a list of memory dumps with address offset
    void append ( const std::vector<MemDump>& list, size_t addAddrOffset )
    {
        for ( const MemDump& addr : list )
            append ( addr, addAddrOffset );
    }

    // Update the list of memory dumps: merge continuous address ranges, and compute total size
    void update();
};
