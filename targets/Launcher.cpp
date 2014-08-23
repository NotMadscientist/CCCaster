#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

using namespace std;


bool hookDLL ( const string& dll_path, const PROCESS_INFORMATION *pi )
{
    HMODULE hKernel32 = GetModuleHandle ( "Kernel32" );
    LPTHREAD_START_ROUTINE pLoadLibrary = ( LPTHREAD_START_ROUTINE ) GetProcAddress ( hKernel32, "LoadLibraryA" );

    void *dll_addr = VirtualAllocEx ( pi->hProcess, 0, dll_path.size() + 1, MEM_COMMIT, PAGE_READWRITE );
    WriteProcessMemory ( pi->hProcess, dll_addr, dll_path.c_str(), dll_path.size() + 1, 0 );

    HANDLE hThread = CreateRemoteThread ( pi->hProcess, 0, 0, pLoadLibrary, dll_addr, 0, 0 );
    if ( !hThread )
    {
        VirtualFreeEx ( pi->hProcess, dll_addr, 0, MEM_RELEASE );
        TerminateProcess ( pi->hProcess, -1 );
        // MessageBox ( 0, "Could not create remote thread.", "launcher error", MB_OK );
        return false;
    }

    HMODULE hookedDLL;

    WaitForSingleObject ( hThread, INFINITE );
    GetExitCodeThread ( hThread, ( DWORD * ) &hookedDLL );
    CloseHandle ( hThread );

    VirtualFreeEx ( pi->hProcess, dll_addr, 0, MEM_RELEASE );

    if ( !hookedDLL )
    {
        TerminateProcess ( pi->hProcess, -1 );
        // MessageBox ( 0, "Could not hook dll after all.", "launcher error", MB_OK );
        return false;
    }

    return true;
}

bool getbase ( HANDLE hnd, DWORD *address, WORD *orig_code )
{
    DWORD peAddress = 0x400000; // FIXME: Let's hope they don't change this
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS32 NTHeader;

    ReadProcessMemory ( hnd, ( LPCVOID ) peAddress, &dosHeader, sizeof ( dosHeader ), 0 );
    ReadProcessMemory ( hnd, ( LPCVOID ) ( peAddress + dosHeader.e_lfanew ), &NTHeader, sizeof ( NTHeader ), 0 );

    *address = peAddress + NTHeader.OptionalHeader.AddressOfEntryPoint;
    ReadProcessMemory ( hnd, ( LPCVOID ) *address, orig_code, 2, 0 );

    return true;
}

// Based on the phook code included with ReplayEx.
bool hook ( const string& exe_path, const string& dll_path )
{
    // Initialize process
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    WIN32_FILE_ATTRIBUTE_DATA exe_info, dat_info;

    memset ( &si, 0, sizeof ( si ) );
    memset ( &pi, 0, sizeof ( pi ) );
    si.cb = sizeof ( si );

    if ( !GetFileAttributesEx ( exe_path.c_str(), GetFileExInfoStandard, &exe_info ) )
    {
        // MessageBox ( 0, "Could not open MBAACC files. This should be contained in the same folder as MBAA.exe.",
        //              "launcher error", MB_OK);
        return false;
    }

    if ( !GetFileAttributesEx ( dll_path.c_str(), GetFileExInfoStandard, &dat_info ) )
    {
        // MessageBox ( 0, dll_path.c_str(), "launcher error", MB_OK | MB_ICONEXCLAMATION );
        return false;
    }

    if ( !CreateProcess ( exe_path.c_str(), 0, 0, 0, TRUE, CREATE_SUSPENDED, 0, 0, &si, &pi ) )
    {
        // MessageBox ( 0, "Could not create process.", "launcher error", MB_OK );
        return false;
    }

    // Wait for process startup, block
    WORD lock_code = 0xfeeb;
    WORD orig_code;
    DWORD address;
    if ( !getbase ( pi.hProcess, &address, &orig_code ) )
    {
        // MessageBox ( 0, "Could not find entry point", "launcher error", MB_OK | MB_ICONEXCLAMATION );
        return false;
    }

    WriteProcessMemory ( pi.hProcess, ( void * ) address, ( char * ) &lock_code, 2, 0 );

    CONTEXT ct;
    ct.ContextFlags = CONTEXT_CONTROL;
    int tries = 0;
    do
    {
        ResumeThread ( pi.hThread );
        Sleep ( 10 );
        SuspendThread ( pi.hThread );

        if ( !GetThreadContext ( pi.hThread, &ct ) )
        {
            if ( tries++ < 500 )
                continue;
            Sleep ( 100 );
            TerminateProcess ( pi.hProcess, -1 );
            // MessageBox ( 0, "Could not get thread context.", "launcher error", MB_OK );
            return false;
        }
    }
    while ( ct.Eip != address );

    if ( !hookDLL ( dll_path, &pi ) )
        return false;

    // Continue normal process action.
    WriteProcessMemory ( pi.hProcess, ( void * ) address, ( char * ) &orig_code, 2, 0 );
    FlushInstructionCache ( pi.hProcess, ( void * ) address, 2 );
    ResumeThread ( pi.hThread );

    return false;
}

int main ( int argc, char *argv[] )
{
    // Create process and hook library.
    if ( argc > 2 && hook ( argv[1], argv[2] ) )
        return 0;

    // MessageBox ( 0, "Could not hook into MBAA.exe\n\nDo you have GameGuard or something running?",
    //              "launcher error", MB_OK | MB_ICONEXCLAMATION);
    return 1;
}
