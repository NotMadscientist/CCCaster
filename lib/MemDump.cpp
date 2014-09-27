#include "MemDump.h"

#include <algorithm>

using namespace std;


static bool compareMemDumpPtrs ( const MemDump *a, const MemDump *b )
{
    return a->addr < b->addr;
}

void MemList::optimize()
{
    // Sort using a list of pointers
    vector<MemDump *> sorted;
    for ( MemDump& addr : addresses )
        sorted.push_back ( &addr );

    sort ( sorted.begin(), sorted.end(), compareMemDumpPtrs );

    vector<MemDump> newAddrs ( 1, sorted.front()->copy() );
    auto it = sorted.begin();
    auto jt = it;

    // Merge continuous address ranges
    for ( ++jt; jt != sorted.end(); ++jt )
    {
        // Add to the previous address's size if the next one is continuous, and neither have children
        if ( newAddrs.back().addr + newAddrs.back().size == ( **jt ).addr )
        {
            // Merge the two addresses
            MemDump tmp = newAddrs.back().copy ( 0, ( **jt ).size );

            if ( tmp.children && ( **jt ).children )
                tmp.children->append ( * ( **jt ).children );
            else if ( ( **jt ).children )
                tmp.children.reset ( new MemList ( * ( **jt ).children ) );

            newAddrs.pop_back();
            newAddrs.push_back ( tmp );

            // Erase and try the next address
            sorted.erase ( jt );
            jt = it;
            continue;
        }

        ++it;
        newAddrs.push_back ( ( **jt ).copy() );
    }

    addresses.clear();
    append ( newAddrs );

    // Update the sizes
    totalSize = 0;

    for ( MemDump& addr : addresses )
    {
        if ( addr.children )
        {
            addr.children->optimize();
            addr.totalSize = addr.size + addr.children->totalSize;
        }

        totalSize += addr.totalSize;
    }
}
