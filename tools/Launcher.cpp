#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <tr1/unordered_set>

using namespace std;
using namespace std::tr1;


static bool popup_errors = true;


bool hookDLL ( const string& dll_path, const PROCESS_INFORMATION *pi )
{
    HMODULE hKernel32 = GetModuleHandle ( "Kernel32" );
    LPTHREAD_START_ROUTINE pLoadLibrary = ( LPTHREAD_START_ROUTINE ) GetProcAddress ( hKernel32, "LoadLibraryA" );

    void *dll_addr = VirtualAllocEx ( pi->hProcess, 0, dll_path.size() + 1, MEM_COMMIT, PAGE_READWRITE );
    WriteProcessMemory ( pi->hProcess, dll_addr, dll_path.c_str(), dll_path.size() + 1, 0 );

    HANDLE hThread = CreateRemoteThread ( pi->hProcess, 0, 0, pLoadLibrary, dll_addr, 0, 0 );
    if ( ! hThread )
    {
        char buffer[4096];
        snprintf ( buffer, sizeof ( buffer ), "Could not create remote thread [%d].", ( int ) GetLastError() );

        if ( popup_errors )
            MessageBox ( 0, buffer, "launcher error", MB_OK );

        // Cleanup
        VirtualFreeEx ( pi->hProcess, dll_addr, 0, MEM_RELEASE );
        TerminateProcess ( pi->hProcess, -1 );
        return false;
    }

    HMODULE hookedDLL;

    WaitForSingleObject ( hThread, INFINITE );
    GetExitCodeThread ( hThread, ( DWORD * ) &hookedDLL );
    CloseHandle ( hThread );

    VirtualFreeEx ( pi->hProcess, dll_addr, 0, MEM_RELEASE );

    if ( ! hookedDLL )
    {
        TerminateProcess ( pi->hProcess, -1 );

        if ( popup_errors )
            MessageBox ( 0, "Could not hook dll", "launcher error", MB_OK );
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
bool hook ( const string& exe_path, const string& dll_path, bool high_priority )
{
    // Initialize process
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    WIN32_FILE_ATTRIBUTE_DATA exe_info, dat_info;

    memset ( &si, 0, sizeof ( si ) );
    memset ( &pi, 0, sizeof ( pi ) );
    si.cb = sizeof ( si );

    if ( ! GetFileAttributesEx ( exe_path.c_str(), GetFileExInfoStandard, &exe_info ) )
    {
        char buffer[4096];
        snprintf ( buffer, sizeof ( buffer ), "Couldn't find exe='%s'\nError [%d].",
                   exe_path.c_str(), ( int ) GetLastError() );

        if ( popup_errors )
            MessageBox ( 0, buffer, "launcher error", MB_OK );
        return false;
    }

    if ( ! GetFileAttributesEx ( dll_path.c_str(), GetFileExInfoStandard, &dat_info ) )
    {
        char buffer[4096];
        snprintf ( buffer, sizeof ( buffer ), "Couldn't find dll='%s'\nError [%d].",
                   dll_path.c_str(), ( int ) GetLastError() );

        if ( popup_errors )
            MessageBox ( 0, buffer, "launcher error", MB_OK );
        return false;
    }

    DWORD flags = CREATE_SUSPENDED;

    if ( high_priority )
        flags |= HIGH_PRIORITY_CLASS;

    char buffer[exe_path.size() + 1];
    strcpy ( buffer, ( "\"" + exe_path + "\"" ).c_str() );

    const string dir_path = exe_path.substr ( 0, exe_path.find_last_of ( "/\\" ) );

    if ( ! CreateProcessA ( 0, buffer, 0, 0, TRUE, flags, 0, dir_path.c_str(), &si, &pi ) )
    {
        char buffer[4096];
        snprintf ( buffer, sizeof ( buffer ), "exe='%s'\ndir='%s'\nCould not create process [%d].",
                   exe_path.c_str(), dir_path.c_str(), ( int ) GetLastError() );

        if ( popup_errors )
            MessageBox ( 0, buffer, "launcher error", MB_OK );
        return false;
    }

    // Wait for process startup, block
    WORD lock_code = 0xfeeb;
    WORD orig_code;
    DWORD address;
    if ( ! getbase ( pi.hProcess, &address, &orig_code ) )
    {
        if ( popup_errors )
            MessageBox ( 0, "Could not find entry point", "launcher error", MB_OK | MB_ICONEXCLAMATION );
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

        if ( ! GetThreadContext ( pi.hThread, &ct ) )
        {
            if ( tries++ < 500 )
                continue;
            Sleep ( 100 );
            TerminateProcess ( pi.hProcess, -1 );

            if ( popup_errors )
                MessageBox ( 0, "Could not get thread context.", "launcher error", MB_OK );
            return false;
        }
    }
    while ( ct.Eip != address );

    if ( ! hookDLL ( dll_path, &pi ) )
        return false;

    // ASM hack to skip the configuration window for MBAA
    static const char jmp1[] = { ( char ) 0xEB, ( char ) 0x0E };
    WriteProcessMemory ( pi.hProcess, ( void * ) 0x04A1D42, jmp1, sizeof ( jmp1 ), 0 );

    static const char jmp2[] = { ( char ) 0xEB };
    WriteProcessMemory ( pi.hProcess, ( void * ) 0x04A1D4A, jmp2, sizeof ( jmp2 ), 0 );

    // Continue normal process action.
    WriteProcessMemory ( pi.hProcess, ( void * ) address, ( char * ) &orig_code, 2, 0 );
    FlushInstructionCache ( pi.hProcess, ( void * ) address, 2 );
    ResumeThread ( pi.hThread );

    return true;
}

int main ( int argc, char *argv[] )
{
    unordered_set<string> options;
    for ( int i = 3; i < argc; ++i )
        options.insert ( string ( argv[i] ) );

    popup_errors = ( options.find ( "--popup_errors" ) != options.end() );

    // Create process and hook library.
    if ( argc > 2 && hook ( argv[1], argv[2], options.find ( "--high" ) != options.end() ) )
        return 0;

    return -1;
}
