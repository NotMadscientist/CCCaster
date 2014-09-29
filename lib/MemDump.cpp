#include "MemDump.h"
#include "Utilities.h"

#include <list>

using namespace std;


static bool compareMemDumpAddrs ( const MemDumpBase& a, const MemDumpBase& b )
{
    return a.getAddr() < b.getAddr();
}

vector<MemDumpPtr> MemDumpBase::setParents ( const vector<MemDumpPtr>& ptrs, const MemDumpBase *parent )
{
    vector<MemDumpPtr> ret;
    ret.reserve ( ptrs.size() );
    for ( const MemDumpPtr& ptr : ptrs )
        ret.push_back ( MemDumpPtr ( parent, ptr.ptrs, ptr.srcOffset, ptr.dstOffset, ptr.size ) );
    return ret;
}

vector<MemDumpPtr> MemDumpBase::addOffsets ( const vector<MemDumpPtr>& ptrs, size_t addSrcOffset )
{
    vector<MemDumpPtr> ret;
    ret.reserve ( ptrs.size() );
    for ( const MemDumpPtr& ptr : ptrs )
        ret.push_back ( MemDumpPtr ( ptr.parent, ptr.ptrs, ptr.srcOffset + addSrcOffset, ptr.dstOffset, ptr.size ) );
    return ret;
}

vector<MemDumpPtr> MemDumpBase::concat ( const vector<MemDumpPtr>& a, const vector<MemDumpPtr>& b )
{
    vector<MemDumpPtr> ret;
    ret.reserve ( a.size() + b.size() );
    for ( const MemDumpPtr& ptr : a )
        ret.push_back ( ptr );
    for ( const MemDumpPtr& ptr : b )
        ret.push_back ( ptr );
    return ret;
}

void MemDumpList::update()
{
    vector<MemDump> sortedVector = sorted ( addrs, compareMemDumpAddrs );
    list<MemDump> sortedList ( sortedVector.begin(), sortedVector.end() );

    addrs.clear();
    addrs.push_back ( sortedList.front() );

    auto it = sortedList.begin();
    auto jt = it;

    // Merge continuous address ranges
    for ( ++jt; jt != sortedList.end(); ++jt )
    {
        // Add to the previous address's size if the next one is continuous
        if ( addrs.back().addr + addrs.back().size == jt->addr )
        {
            // Merge the two addresses
            MemDump merged ( addrs.back(), *jt );
            addrs.pop_back();
            addrs.push_back ( merged );

            // Erase and try the next address
            sortedList.erase ( jt );
            jt = it;
            continue;
        }

        ++it;
        addrs.push_back ( *jt );
    }

    // Update the sizes
    totalSize = 0;
    for ( const MemDump& mem : addrs )
        totalSize += mem.getTotalSize();
}
