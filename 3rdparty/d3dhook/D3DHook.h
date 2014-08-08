#pragma once

#include <string>

struct IDirect3DDevice9;

// Need to be implemented if the the hooks are to be used
void PresentFrameBegin ( IDirect3DDevice9 *device );
void PresentFrameEnd ( IDirect3DDevice9 *device );
void InvalidateDeviceObjects();

// Initialize DX from the provided window handle, returns a non-empty error string on failure
std::string InitDirectX ( void *hwnd );

// Hook the above DX functions, returns a non-empty error string on failure
std::string HookDirectX();

// Unhook the hooked DX functions
void UnhookDirectX();
