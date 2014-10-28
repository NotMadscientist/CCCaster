#include "MemDump.h"
#include "Utilities.h"
#include "Compression.h"

#include <list>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <fstream>

using namespace std;
using namespace cereal;


static bool compareMemDumpAddrs ( const MemDumpBase& a, const MemDumpBase& b )
{
    return ( a.getAddr() < b.getAddr() );
}

void MemDumpBase::saveDump ( char *&dump ) const
{
    ASSERT ( dump != 0 );

    const char *addr = getAddr();

    if ( addr )
        copy ( addr, addr + size, dump );
    else
        memset ( dump, 0, size );

    dump += size;

    for ( const MemDumpPtr& ptr : ptrs )
        ptr.saveDump ( dump );
}

void MemDumpBase::loadDump ( const char *&dump ) const
{
    ASSERT ( dump != 0 );

    char *addr = getAddr();

    if ( addr )
        copy ( dump, dump + size, addr );

    dump += size;

    for ( const MemDumpPtr& ptr : ptrs )
        ptr.loadDump ( dump );
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

    // Update the total size
    totalSize = 0;
    for ( const MemDump& mem : addrs )
        totalSize += mem.getTotalSize();
}

void MemDumpBase::save ( BinaryOutputArchive& ar ) const
{
    ar ( size, ptrs.size() );
    for ( const MemDumpPtr& ptr : ptrs )
        ptr.save ( ar );
}

void MemDumpPtr::save ( BinaryOutputArchive& ar ) const
{
    ar ( srcOffset, dstOffset );
    MemDumpBase::save ( ar );
}

void MemDump::save ( BinaryOutputArchive& ar ) const
{
    uint32_t val = ( uint32_t ) addr;
    ar ( val );
    MemDumpBase::save ( ar );
}

void MemDumpList::save ( BinaryOutputArchive& ar ) const
{
    ar ( totalSize, addrs.size() );
    for ( const MemDump& mem : addrs )
        mem.save ( ar );
}

static vector<MemDumpPtr> loadPtrs ( size_t count, BinaryInputArchive& ar )
{
    vector<MemDumpPtr> ret;
    ret.reserve ( count );

    for ( size_t i = 0; i < count; ++i )
    {
        size_t srcOffset, dstOffset, size, ptrsCount;
        ar ( srcOffset, dstOffset, size, ptrsCount );

        if ( ptrsCount )
            ret.push_back ( MemDumpPtr ( srcOffset, dstOffset, size, loadPtrs ( ptrsCount, ar ) ) );
        else
            ret.push_back ( MemDumpPtr ( srcOffset, dstOffset, size ) );
    }

    return ret;
}

void MemDumpList::load ( BinaryInputArchive& ar )
{
    size_t count;
    ar ( totalSize, count );

    for ( size_t i = 0; i < count; ++i )
    {
        uint32_t addr;
        size_t size, ptrsCount;
        ar ( addr, size, ptrsCount );

        if ( ptrsCount )
            append ( { ( char * ) addr, size, loadPtrs ( ptrsCount, ar ) } );
        else
            append ( { ( char * ) addr, size } );
    }
}

bool MemDumpList::save ( const char *filename ) const
{
    string data;

    // Try to serialize this class
    try
    {
        ostringstream ss ( ostringstream::binary );
        BinaryOutputArchive archive ( ss );
        save ( archive );
        data = ss.str();
    }
    catch ( ... )
    {
        // TODO LOG OR THROW
        return false;
    }

    // Compute MD5
    char md5[16];
    getMD5 ( data, md5 );
    data.append ( md5, sizeof ( md5 ) );

    // Try to write to the file
    ofstream fout ( filename, ofstream::binary );
    bool good = fout.good();
    if ( good )
        good = fout.write ( &data[0], data.size() ).good();
    fout.close();
    return good;
}

bool MemDumpList::load ( const char *filename )
{
    string data;

    // Open file
    ifstream fin ( filename, ifstream::binary );
    bool good = fin.good();

    if ( good )
    {
        // Get the length of the file
        fin.seekg ( 0, fin.end );
        size_t length = fin.tellg();
        fin.seekg ( 0, fin.beg );

        // Try to read from the file
        data.resize ( length, 0 );
        fin.read ( &data[0], data.size() );
    }

    fin.close();

    if ( !good )
    {
        // TODO LOG OR THROW
        return false;
    }

    if ( data.size() <= 16 )
    {
        // TODO LOG OR THROW
        return false;
    }

    // Check MD5
    char md5[16];
    copy ( data.begin() + ( data.size() - 16 ), data.end(), md5 );
    data.resize ( data.size() - 16 );

    if ( ! checkMD5 ( data, md5 ) )
    {
        // TODO LOG OR THROW
        return false;
    }

    // Try to deserialize this class
    try
    {
        istringstream ss ( data, ostringstream::binary );
        BinaryInputArchive archive ( ss );
        load ( archive );
    }
    catch ( ... )
    {
        // TODO LOG OR THROW
        return false;
    }

    return true;
}
