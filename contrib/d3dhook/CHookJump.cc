//
// CHookJump.cpp
//
#include <windows.h>
#include "CHookJump.h"

bool CHookJump::InstallHook ( LPVOID pFunc, LPVOID pFuncNew )
{
    assert ( pFuncNew );
    if ( pFunc == NULL )
    {
        // DEBUG_ERR(("InstallHook: NULL."));
        return false;
    }
    if ( IsHookInstalled() && ! memcmp ( pFunc, m_Jump, sizeof ( m_Jump ) ) )
    {
        // DEBUG_MSG(("InstallHook: already has JMP-implant." LOG_CR));
        return true;
    }

    // DEBUG_TRACE(("InstallHook: pFunc = %08x, pFuncNew = %08x" LOG_CR, (UINT_PTR)pFunc, (UINT_PTR)pFuncNew ));
    m_Jump[0] = 0;

    DWORD dwNewProtection = PAGE_EXECUTE_READWRITE;
    if ( ! ::VirtualProtect ( pFunc, 8, dwNewProtection, &m_dwOldProtection ) )
    {
        assert ( 0 );
        return false;
    }

    // unconditional JMP to relative address is 5 bytes.
    m_Jump[0] = 0xe9;
    DWORD dwAddr = ( DWORD ) ( ( UINT_PTR ) pFuncNew - ( UINT_PTR ) pFunc ) - sizeof ( m_Jump );
    // DEBUG_TRACE(("JMP %08x" LOG_CR, dwAddr ));
    memcpy ( m_Jump + 1, &dwAddr, sizeof ( dwAddr ) );

    memcpy ( m_OldCode, pFunc, sizeof ( m_OldCode ) );
    memcpy ( pFunc, m_Jump, sizeof ( m_Jump ) );

    // DEBUG_MSG(("InstallHook: JMP-hook planted." LOG_CR));
    return true;
}

void CHookJump::RemoveHook ( LPVOID pFunc )
{
    if ( pFunc == NULL )
        return;
    if ( ! IsHookInstalled() )  // was never set!
        return;
    try
    {
        memcpy ( pFunc, m_OldCode, sizeof ( m_OldCode ) ); // SwapOld(pFunc)
        DWORD dwOldProtection = 0;
        ::VirtualProtect ( pFunc, 8, m_dwOldProtection, &dwOldProtection ); // restore protection.
        m_Jump[0] = 0;  // destroy my jump. (must reconstruct it)
    }
    catch ( ... )
    {
        // DEBUG_ERR(("CHookJump::RemoveHook FAIL" LOG_CR));
    }
}

