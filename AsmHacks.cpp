#include "AsmHacks.h"

#include <windows.h>


int memwrite ( void *dst, const void *src, size_t len )
{
    DWORD old, tmp;

    if ( !VirtualProtect ( dst, len, PAGE_READWRITE, &old ) )
        return GetLastError();

    memcpy ( dst, src, len );

    if ( !VirtualProtect ( dst, len, old, &tmp ) )
        return GetLastError();

    return 0;
}

namespace AsmHacks
{

int Asm::write() const
{
    return memwrite ( addr, &bytes[0], bytes.size() );
}

} // namespace AsmHacks
