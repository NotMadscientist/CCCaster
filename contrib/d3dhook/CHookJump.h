//
// CHookJump.h
//
#pragma once
#include <cassert>

struct CHookJump
{
    // Create a relative jump to hook an existing api call.
    // is this 5 bytes in 64 bit mode as well?
    // NOTE: NOT for use in hooking an interface or a vtable. that doesnt require a jump.
#define CHookJump_LEN 5
public:
    CHookJump()
        : m_dwOldProtection ( 0 )
    {
        m_Jump[0] = 0;
    }

    bool IsHookInstalled() const
    {
        return ( m_Jump[0] != 0 );
    }
    bool InstallHook ( LPVOID pFunc, LPVOID pFuncNew );
    void RemoveHook ( LPVOID pFunc );

    void SwapOld ( LPVOID pFunc )
    {
        // put back saved code fragment
        assert ( pFunc );
        memcpy ( pFunc, m_OldCode, sizeof ( m_OldCode ) );
    }
    void SwapReset ( LPVOID pFunc )
    {
        // put back JMP instruction again
        if ( ! IsHookInstalled() )  // hook has since been destroyed!
            return;
        assert ( pFunc );
        memcpy ( pFunc, m_Jump, sizeof ( m_Jump ) );
    }
public:
    DWORD m_dwOldProtection;            // used by VirtualProtect()
    BYTE m_OldCode[CHookJump_LEN];  // what was there previous.
    BYTE m_Jump[CHookJump_LEN];     // what do i want to replace it with.
};

