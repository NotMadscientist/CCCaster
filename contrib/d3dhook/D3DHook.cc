#include "D3DHook.h"

#include <IRefPtr.h>
#include <CDllFile.h>
#include <CHookJump.h>

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <comdef.h>

#include <cassert>
#include <sstream>

using namespace std;

template<typename T>
inline string toHexString ( const T& val )
{
    stringstream ss;
    ss << hex << val;
    return ss.str();
}

// DX interface entry offsets.
#define INTF_QueryInterface 0
#define INTF_AddRef         1
#define INTF_Release        2
#define INTF_DX9_Reset      16
#define INTF_DX9_Present    17

typedef IDirect3D9 * ( __stdcall *DIRECT3DCREATE9 ) ( UINT );
typedef ULONG   ( __stdcall *PFN_DX9_ADDREF ) ( IDirect3DDevice9 *pDevice );
typedef ULONG   ( __stdcall *PFN_DX9_RELEASE ) ( IDirect3DDevice9 *pDevice );
typedef HRESULT ( __stdcall *PFN_DX9_RESET ) ( IDirect3DDevice9 *pDevice, LPVOID );
typedef HRESULT ( __stdcall *PFN_DX9_PRESENT ) ( IDirect3DDevice9 *pDevice, const RECT *, const RECT *, HWND, LPVOID );

CDllFile g_DX9;
IDirect3DDevice9 *g_pDevice = 0;
ULONG g_iRefCount = 1;
ULONG g_iRefCountMe = 0;
UINT_PTR m_nDX9_Present;
UINT_PTR m_nDX9_Reset;

CHookJump m_Hook_Present;
CHookJump m_Hook_Reset;
UINT_PTR *m_Hook_AddRef = 0;
UINT_PTR *m_Hook_Release = 0;

PFN_DX9_ADDREF  s_D3D9_AddRef = 0;
PFN_DX9_RELEASE s_D3D9_Release = 0;
PFN_DX9_RESET   s_D3D9_Reset = 0;
PFN_DX9_PRESENT s_D3D9_Present = 0;

EXTERN_C ULONG __declspec ( dllexport ) __stdcall DX9_AddRef ( IDirect3DDevice9 *pDevice )
{
    // New AddRef function
    g_iRefCount = s_D3D9_AddRef ( pDevice );
    // DEBUG_TRACE(("DX9_AddRef: called (m_iRefCount = %d)." LOG_CR, g_iRefCount));
    return g_iRefCount;
}

EXTERN_C ULONG __declspec ( dllexport ) __stdcall DX9_Release ( IDirect3DDevice9 *pDevice )
{
    // New Release function
    // a "fall-through" case
    if ( ( g_iRefCount > g_iRefCountMe + 1 ) && s_D3D9_Release )
    {
        g_iRefCount = s_D3D9_Release ( pDevice );
        // DEBUG_TRACE(("DX9_Release: called (m_iRefCount = %d)." LOG_CR, g_iRefCount));
        return g_iRefCount;
    }

    /*
    DEBUG_TRACE(("+++++++++++++++++++++++++++++++++++++" LOG_CR ));
    DEBUG_MSG(("DX9_Release: called." LOG_CR));
    DEBUG_TRACE(("DX9_Release: pDevice = %08x" LOG_CR, (UINT_PTR)pDevice));
    DEBUG_TRACE(("DX9_Release: VTABLE[0] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[0]));
    DEBUG_TRACE(("DX9_Release: VTABLE[1] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[1]));
    DEBUG_TRACE(("DX9_Release: VTABLE[2] = %08x" LOG_CR, ((UINT_PTR*)(*((UINT_PTR*)pDevice)))[2]));
    */

    g_pDevice = pDevice;

    // unhook device methods
    UnhookDirectX();

    // reset the pointers
    m_Hook_AddRef = 0;
    m_Hook_Release = 0;

    // call the real Release()
    // DEBUG_MSG(( "DX9_Release: about to call real Release." LOG_CR));

    g_iRefCount = s_D3D9_Release ( pDevice );
    // DEBUG_MSG(( "DX9_Release: UNHOOK m_iRefCount = %d" LOG_CR, g_iRefCount));
    return g_iRefCount;
}

void DX9_HooksInit ( IDirect3DDevice9 *pDevice )
{
    UINT_PTR *pVTable = ( UINT_PTR * ) ( * ( ( UINT_PTR * ) pDevice ) );
    assert ( pVTable );
    m_Hook_AddRef = pVTable + 1;
    m_Hook_Release = pVTable + 2;

    // DEBUG_TRACE(("*m_Hook_AddRef  = %08x" LOG_CR, *g_DX9.m_Hook_AddRef));
    // DEBUG_TRACE(("*m_Hook_Release  = %08x" LOG_CR, *g_DX9.m_Hook_Release));

    // hook AddRef method
    s_D3D9_AddRef = ( PFN_DX9_ADDREF ) ( *m_Hook_AddRef );
    *m_Hook_AddRef = ( UINT_PTR ) DX9_AddRef;

    // hook Release method
    s_D3D9_Release = ( PFN_DX9_RELEASE ) ( *m_Hook_Release );
    *m_Hook_Release = ( UINT_PTR ) DX9_Release;
}

void DX9_HooksVerify ( IDirect3DDevice9 *pDevice )
{
    // It looks like at certain points, vtable entries get restored to its original values.
    // If that happens, we need to re-assign them to our functions again.
    // NOTE: we don't want blindly re-assign, because there can be other programs
    // hooking on the same methods. Therefore, we only re-assign if we see that
    // original addresses are restored by the system.

    UINT_PTR *pVTable = ( UINT_PTR * ) ( * ( ( UINT_PTR * ) pDevice ) );
    assert ( pVTable );
    if ( pVTable[INTF_AddRef] == ( UINT_PTR ) s_D3D9_AddRef )
    {
        pVTable[INTF_AddRef] = ( UINT_PTR ) DX9_AddRef;
        // DEBUG_MSG(( "DX9_HooksVerify: pDevice->AddRef() re-hooked." LOG_CR));
    }
    if ( pVTable[INTF_Release] == ( UINT_PTR ) s_D3D9_Release )
    {
        pVTable[INTF_Release] = ( UINT_PTR ) DX9_Release;
        // DEBUG_MSG(( "DX9_HooksVerify: pDevice->Release() re-hooked." LOG_CR));
    }
}

EXTERN_C HRESULT __declspec ( dllexport ) __stdcall DX9_Reset ( IDirect3DDevice9 *pDevice, LPVOID params )
{
    // New Reset function
    // put back saved code fragment
    m_Hook_Reset.SwapOld ( ( void * ) s_D3D9_Reset );

    // LOG(( "DX9_Reset: called." LOG_CR));

    g_pDevice = pDevice;

    InvalidateDeviceObjects();

    // call real Reset()
    HRESULT hRes = s_D3D9_Reset ( pDevice, params );

    DX9_HooksVerify ( pDevice );

    // DEBUG_MSG(( "DX9_Reset: done." LOG_CR));

    // put JMP instruction again
    m_Hook_Reset.SwapReset ( ( void * ) s_D3D9_Reset );
    return hRes;
}

EXTERN_C HRESULT __declspec ( dllexport ) __stdcall DX9_Present (
    IDirect3DDevice9 *pDevice, const RECT *src, const RECT *dest, HWND hwnd, LPVOID unused )
{
    // New Present function
    m_Hook_Present.SwapOld ( ( void * ) s_D3D9_Present );

    // DEBUG_TRACE(( "--------------------------------" LOG_CR ));
    // DEBUG_TRACE(( "DX9_Present: called." LOG_CR ));

    g_pDevice = pDevice;

    // rememeber IDirect3DDevice9::Release pointer so that we can clean-up properly.
    if ( m_Hook_AddRef == 0 || m_Hook_Release == 0 )
    {
        DX9_HooksInit ( pDevice );
    }

    PresentFrameBegin ( pDevice );

    // call real Present()
    HRESULT hRes = s_D3D9_Present ( pDevice, src, dest, hwnd, unused );

    PresentFrameEnd ( pDevice );

    DX9_HooksVerify ( pDevice );
    // DEBUG_TRACE(( "DX9_Present: done." LOG_CR ));

    m_Hook_Present.SwapReset ( ( void * ) s_D3D9_Present );
    return hRes;
}

string InitDirectX ( void *hwnd )
{
    // get the offset from the start of the DLL to the interface element we want.
    // step 1: Load d3d9.dll
    HRESULT hRes = g_DX9.LoadDll ( TEXT ( "d3d9" ) );
    if ( IS_ERROR ( hRes ) )
    {
        _com_error err ( hRes );
        return "Failed to load d3d9.dll: [" + toHexString ( hRes ) + "] " + err.ErrorMessage();
    }

    // step 2: Get IDirect3D9
    DIRECT3DCREATE9 pDirect3DCreate9 = ( DIRECT3DCREATE9 ) g_DX9.GetProcAddress ( "Direct3DCreate9" );
    if ( pDirect3DCreate9 == 0 )
    {
        return "Unable to find Direct3DCreate9";
    }

    IRefPtr<IDirect3D9> pD3D = pDirect3DCreate9 ( D3D_SDK_VERSION );
    if ( !pD3D.IsValidRefObj() )
    {
        return "Direct3DCreate9 failed";
    }

    // step 3: Get IDirect3DDevice9
    D3DDISPLAYMODE d3ddm;
    hRes = pD3D->GetAdapterDisplayMode ( D3DADAPTER_DEFAULT, &d3ddm );
    if ( FAILED ( hRes ) )
    {
        _com_error err ( hRes );
        return "GetAdapterDisplayMode failed: [" + toHexString ( hRes ) + "] " + err.ErrorMessage();
    }

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory ( &d3dpp, sizeof ( d3dpp ) );
    d3dpp.Windowed = true;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    IRefPtr<IDirect3DDevice9> pD3DDevice;
    hRes = pD3D->CreateDevice ( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ( HWND ) hwnd,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                &d3dpp, IREF_GETPPTR ( pD3DDevice, IDirect3DDevice9 ) );
    if ( FAILED ( hRes ) )
    {
        _com_error err ( hRes );
        return "CreateDevice failed: [" + toHexString ( hRes ) + "] " + err.ErrorMessage();
    }

    // step 4: store method addresses in out vars
    UINT_PTR *pVTable = ( UINT_PTR * ) ( * ( ( UINT_PTR * ) pD3DDevice.get_RefObj() ) );
    assert ( pVTable );
    m_nDX9_Present = ( pVTable[INTF_DX9_Present] - g_DX9.get_DllInt() );
    m_nDX9_Reset = ( pVTable[INTF_DX9_Reset] - g_DX9.get_DllInt() );

    // LOG ( "InitDirectX: %08x, Present=0%x, Reset=0%x ",
    //       ( UINT_PTR ) pD3DDevice.get_RefObj(), m_nDX9_Present, m_nDX9_Reset );
    return "";
}

string HookDirectX()
{
    // This function hooks two IDirect3DDevice9 methods, using code overwriting technique.
    // hook IDirect3DDevice9::Present(), using code modifications at run-time.
    // ALGORITHM: we overwrite the beginning of real IDirect3DDevice9::Present
    // with a JMP instruction to our routine (DX9_Present).
    // When our routine gets called, first thing that it does - it restores
    // the original bytes of IDirect3DDevice9::Present, then performs its pre-call tasks,
    // then calls IDirect3DDevice9::Present, then does post-call tasks, then writes
    // the JMP instruction back into the beginning of IDirect3DDevice9::Present, and
    // returns.

    if ( !m_nDX9_Present || !m_nDX9_Reset )
        return "No info on 'Present' and/or 'Reset'";

    s_D3D9_Present = ( PFN_DX9_PRESENT ) ( g_DX9.get_DllInt() + m_nDX9_Present );
    s_D3D9_Reset = ( PFN_DX9_RESET ) ( g_DX9.get_DllInt() + m_nDX9_Reset );

    if ( !m_Hook_Present.InstallHook ( ( void * ) s_D3D9_Present, ( void * ) DX9_Present ) )
        return "m_Hook_Present failed";

    if ( !m_Hook_Reset.InstallHook ( ( void * ) s_D3D9_Reset, ( void * ) DX9_Reset ) )
        return "m_Hook_Reset failed";

    return "";
}

void UnhookDirectX()
{
    // Restore original Reset() and Present()
    if ( m_Hook_AddRef != 0 && s_D3D9_AddRef != 0 )
    {
        *m_Hook_AddRef = ( UINT_PTR ) s_D3D9_AddRef;
    }
    if ( m_Hook_Release != 0 && s_D3D9_Release != 0 )
    {
        *m_Hook_Release = ( UINT_PTR ) s_D3D9_Release;
    }

    // restore IDirect3D9Device methods
    m_Hook_Present.RemoveHook ( ( void * ) s_D3D9_Present );
    m_Hook_Reset.RemoveHook ( ( void * ) s_D3D9_Reset );

    InvalidateDeviceObjects();
}
