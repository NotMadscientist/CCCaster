#include "DllAsmHacks.h"
#include "Messages.h"
#include "DllNetplayManager.h"
#include "CharacterSelect.h"
#include "Logger.h"

#include <windows.h>

using namespace std;


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

uint32_t roundStartCounter = 0;

uint32_t *autoReplaySaveStatePtr = 0;

uint8_t enableEscapeToExit = true;

uint8_t sfxFilterArray[CC_SFX_ARRAY_LEN] = { 0 };

uint8_t sfxMuteArray[CC_SFX_ARRAY_LEN] = { 0 };


extern "C" void charaSelectColorCb()
{
    uint32_t *edi;

    asm ( "movl %%edi,%0" : "=r" ( edi ) );

    Sleep ( 20 ); // This is code that was replaced

    const uint32_t *ptrBase = ( uint32_t * ) 0x74D808;

    if ( ! *ptrBase )
        return;

    const uint32_t *ptr1 = ( uint32_t * ) ( *ptrBase + 0x1AC ); // P1 color table reference
    const uint32_t *ptr2 = ( uint32_t * ) ( *ptrBase + 0x388 ); // P2 color table reference

    LOG ( "edi=%08X; ptr1=%08X; *ptr1=%08X; ptr2=%08X; *ptr2=%08X", edi, ptr1, *ptr1, ptr2, *ptr2 );

    if ( edi + 1 == ptr1 && *ptr1 )
    {
        colorLoadCallback ( 1, ( ( uint32_t * ) *ptr1 ) + 1 );
    }
    else if ( edi + 1 == ptr2 && *ptr2 )
    {
        colorLoadCallback ( 2, ( ( uint32_t * ) *ptr2 ) + 1 );
    }
}

extern "C" void loadingStateColorCb()
{
    uint32_t *eax, *esi;

    asm ( "movl %%eax,%0" : "=r" ( eax ) );
    asm ( "movl %%esi,%0" : "=r" ( esi ) );

    uint32_t *ptr = ( uint32_t * ) ( ( uint32_t ( esi ) << 10 ) + uint32_t ( eax ) + 4 );

    LOG ( "eax=%08X; esi=%08X; ptr=%08X", eax, esi, ptr );

    // TODO make this actually fill in the correct character (taking into account the team characters)
    colorLoadCallback ( 1, 0, 0, ptr );
}

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
