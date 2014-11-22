#include "AsmHacks.h"

#include <windows.h>


static int memwrite ( void *dst, const void *src, size_t len )
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

uint32_t currentMenuIndex = 0;

uint32_t menuConfirmState = 0;

uint32_t *charaSelectModePtrs[2] = { 0, 0 };

uint32_t roundStartCounter = 0;

uint32_t *autoReplaySaveStatePtr = 0;


int Asm::write() const
{
    backup.resize ( bytes.size() );
    memcpy ( &backup[0], addr, backup.size() );
    return memwrite ( addr, &bytes[0], bytes.size() );
}

int Asm::revert() const
{
    return memwrite ( addr, &backup[0], backup.size() );
}

} // namespace AsmHacks
