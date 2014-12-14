/*
 *  MinHook - Minimalistic API Hook Library
 *  Copyright (C) 2009 Tsuda Kageyu. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <windows.h>

#include <string>

// MinHook Error Codes.
typedef enum MH_STATUS
{
	// Unknown error. Should not be returned.
	MH_UNKNOWN = -1,

	// Successful.
	MH_OK = 0,

	// MinHook is already initialized.
	MH_ERROR_ALREADY_INITIALIZED,

	// MinHook is not initialized yet, or already uninitialized.
	MH_ERROR_NOT_INITIALIZED,

	// The hook for the specified target function is already created.
	MH_ERROR_ALREADY_CREATED,

	// The hook for the specified target function is not created yet.
	MH_ERROR_NOT_CREATED,

	// The hook for the specified target function is already enabled.
	MH_ERROR_ENABLED,

	// The hook for the specified target function is not enabled yet, or already disabled.
	MH_ERROR_DISABLED,

	// The specified pointer is invalid. It points to the address of non-allocated and/or non-executable region.
	MH_ERROR_NOT_EXECUTABLE,

	// The specified target function cannot be hooked.
	MH_ERROR_UNSUPPORTED_FUNCTION,

	// Failed to allocate memory.
	MH_ERROR_MEMORY_ALLOC,

	// Failed to change the memory protection.
	MH_ERROR_MEMORY_PROTECT
}
MH_STATUS;

inline const char *MH_StatusString ( MH_STATUS status )
{
	switch ( status )
	{
		default:
		case MH_UNKNOWN:
			return "[MH_UNKNOWN] Unknown error. Should not be returned.";

		case MH_OK:
			return "[MH_OK] Successful.";

		case MH_ERROR_ALREADY_INITIALIZED:
			return "[MH_ERROR_ALREADY_INITIALIZED] MinHook is already initialized.";

		case MH_ERROR_NOT_INITIALIZED:
			return "[MH_ERROR_NOT_INITIALIZED] MinHook is not initialized yet, or already uninitialized.";

		case MH_ERROR_ALREADY_CREATED:
			return "[MH_ERROR_ALREADY_CREATED] The hook for the specified target function is already created.";

		case MH_ERROR_NOT_CREATED:
			return "[MH_ERROR_NOT_CREATED] The hook for the specified target function is not created yet.";

		case MH_ERROR_ENABLED:
			return "[MH_ERROR_ENABLED] The hook for the specified target function is already enabled.";

		case MH_ERROR_DISABLED:
			return "[MH_ERROR_DISABLED] The hook for the specified target function is not enabled yet: or already disabled.";

		case MH_ERROR_NOT_EXECUTABLE:
			return "[MH_ERROR_NOT_EXECUTABLE] The specified pointer is invalid. It points to the address of non-allocated and/or non-executable region.";

		case MH_ERROR_UNSUPPORTED_FUNCTION:
			return "[MH_ERROR_UNSUPPORTED_FUNCTION] The specified target function cannot be hooked.";

		case MH_ERROR_MEMORY_ALLOC:
			return "[MH_ERROR_MEMORY_ALLOC] Failed to allocate memory.";

		case MH_ERROR_MEMORY_PROTECT:
			return "[MH_ERROR_MEMORY_PROTECT] Failed to change the memory protection.";
	}
}

// Can be passed as a parameter to MH_EnableHook, MH_DisableHook, MH_QueueEnableHook or MH_QueueDisableHook.
#define MH_ALL_HOOKS NULL

#if defined __cplusplus
extern "C" {
#endif

	// Initialize the MinHook library.
	MH_STATUS WINAPI MH_Initialize();

	// Uninitialize the MinHook library.
	MH_STATUS WINAPI MH_Uninitialize();

	// Creates the Hook for the specified target function, in disabled state.
	// Parameters:
	//   pTarget    [in]  A pointer to the target function, which will be overridden by the detour function.
	//   pDetour    [in]  A pointer to the detour function, which will override the target function.
	//   ppOriginal [out] A pointer to the trampoline function, which will be used to call the original target function.
	MH_STATUS WINAPI MH_CreateHook(void* pTarget, void* const pDetour, void** ppOriginal = 0);

	// Removes the already created hook.
	// Parameters:
	//   pTarget [in] A pointer to the target function.
	MH_STATUS WINAPI MH_RemoveHook(void* pTarget);

	// Enables the already created hook.
	// Parameters:
	//   pTarget [in] A pointer to the target function.
	//                If this parameter is MH_ALL_HOOKS, all created hooks are enabled in one go.
	MH_STATUS WINAPI MH_EnableHook(void* pTarget);

	// Disables the already created hook.
	// Parameters:
	//   pTarget [in] A pointer to the target function.
	//                If this parameter is MH_ALL_HOOKS, all created hooks are disabled in one go.
	MH_STATUS WINAPI MH_DisableHook(void* pTarget);

	// Queues to enable the already created hook.
	// Parameters:
	//   pTarget [in] A pointer to the target function.
	//                If this parameter is MH_ALL_HOOKS, all created hooks are queued to be enabled.
	MH_STATUS WINAPI MH_QueueEnableHook(void* pTarget);

	// Queues to disable the already created hook.
	// Parameters:
	//   pTarget [in] A pointer to the target function.
	//                If this parameter is MH_ALL_HOOKS, all created hooks are queued to be disabled.
	MH_STATUS WINAPI MH_QueueDisableHook(void* pTarget);

	// Applies all queued changes in one go.
	MH_STATUS WINAPI MH_ApplyQueued();

#if defined __cplusplus
}
#endif

// Convenience macros to generate function type, pointer, and definition for Windows functions.
//
// Example usage:
//
// pQueryPerformanceCounter is the function pointer type
//
// oQueryPerformanceCounter is the original function
//
// mQueryPerformanceCounter is the hooked function
//
// MH_WINAPI_HOOK ( BOOL, WINAPI, QueryPerformanceCounter, LARGE_INTEGER *lpPerformanceCount )
// {
// 		return oQueryPerformanceCounter ( lpPerformanceCount );
// }
//
// MH_CREATE_HOOK ( QueryPerformanceCounter );
//
// MH_REMOVE_HOOK ( QueryPerformanceCounter );

#define MH_WINAPI_HOOK(RETURN_TYPE, PREFIX, FUNC_NAME, ...)                     \
    typedef RETURN_TYPE ( PREFIX *p ## FUNC_NAME ) ( __VA_ARGS__ );             \
    p ## FUNC_NAME o ## FUNC_NAME = 0;                                          \
    RETURN_TYPE PREFIX m ## FUNC_NAME ( __VA_ARGS__ )

#define MH_CREATE_HOOK(FUNC_NAME) \
    MH_CreateHook ( ( void * ) FUNC_NAME, ( void * ) m ## FUNC_NAME, ( void ** ) &o ## FUNC_NAME )

#define MH_REMOVE_HOOK(FUNC_NAME) MH_RemoveHook ( ( void * ) FUNC_NAME )
