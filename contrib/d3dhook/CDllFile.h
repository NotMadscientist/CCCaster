//
// CDll.h
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
// Manages links to a *.dll file.
//
#ifndef _INC_CDll_H
#define _INC_CDll_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32

#define FILE_EXT_DLL ".dll"

#include <cassert>

struct CDllFile
{
    // manage access to a *.DLL file.
    // Inside a DLL there may be procedures and resources.
    friend class CDllFuncBase;
public:
    CDllFile ( HMODULE hModule = NULL, bool bUnload = true )
        : m_hModule ( hModule )
        , m_bUnload ( bUnload )
    {
    }
    ~CDllFile()
    {
        FreeDllLast();
    }

    bool IsValidDll() const
    {
        return ( m_hModule != NULL );
    }
    operator HMODULE() const
    {
        return m_hModule;
    }
    HMODULE get_DllHandle() const
    {
        return m_hModule;
    }
    UINT_PTR get_DllInt() const
    {
        return ( UINT_PTR ) m_hModule;
    }
    HMODULE DetachModule()
    {
        HMODULE hModule = m_hModule;
        m_hModule = NULL;
        return hModule;
    }

    void FreeDll()
    {
        if ( m_hModule == NULL )
            return;
        if ( m_bUnload )
        {
            ::FreeLibrary ( m_hModule );
        }
        m_hModule = NULL;
    }
    bool FindDll ( const TCHAR *pszModuleName )
    {
        // is the Dll already loaded?
        if ( m_hModule && m_bUnload )
        {
            return m_hModule ? true : false;
        }
        m_hModule = ::GetModuleHandle ( pszModuleName );
        if ( ! IsValidDll() )
            return false;
        m_bUnload = false;
        return true;
    }
    HRESULT LoadDllEx ( const TCHAR *pszModuleName, UINT uFlags = 0 )
    {
        // CGFile::GetLastError() for HRESULT code.
        // uFlags = DONT_RESOLVE_DLL_REFERENCES = prevent calling of DLL_PROCESS_ATTACH?
        FreeDllLast();
        m_hModule = ::LoadLibraryEx ( pszModuleName, NULL, uFlags );  // file name of module
        if ( m_hModule == NULL )
        {
            return HRESULT_FROM_WIN32 ( ::GetLastError() );
        }
        m_bUnload = true;
        return S_OK;
    }
    HRESULT LoadDll ( const TCHAR *pszModuleName )
    {
        // CGFile::GetLastError() for HRESULT code.
        FreeDllLast();
        m_hModule = ::LoadLibrary ( pszModuleName );  // file name of module
        if ( m_hModule == NULL )
        {
            return HRESULT_FROM_WIN32 ( ::GetLastError() );
        }
        m_bUnload = true;
        return S_OK;
    }
    FARPROC GetProcAddress ( const char *pszFuncName ) const
    {
        // Get a Generic function call. assume nothing about args.
        // NOTE: No such thing as a UNICODE proc name! object formats existed before unicode.
        assert ( m_hModule );
        return ::GetProcAddress ( m_hModule, pszFuncName );
    }

    // LoadResource()
#ifdef _DEBUG
    static bool _stdcall UnitTest();
#endif

protected:
    void FreeDllLast()
    {
        if ( m_hModule == NULL )
            return;
        if ( ! m_bUnload )
            return;
        ::FreeLibrary ( m_hModule );
    }
private:
    HMODULE m_hModule;  // sometimes the same as HINSTANCE ?
    bool m_bUnload;
};

#endif // _WIN32
#endif // _INC_CDLL_H
