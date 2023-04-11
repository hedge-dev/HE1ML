#include "Globals.h"
#include <d3d9.h>

namespace
{
	byte state_flags;
}

VTABLE_HOOK(HRESULT, WINAPI, IDirect3DDevice9, Present, 17, void* pSrcRect, void* pDestRect, HWND window, void* pDirtyRegion)
{
	g_loader->OnUpdate();
	return originalIDirect3DDevice9Present(This, pSrcRect, pDestRect, window, pDirtyRegion);
}

VTABLE_HOOK(HRESULT, WINAPI, IDirect3DDevice9Ex, PresentEx, 121, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
	g_loader->OnUpdate();
	return originalIDirect3DDevice9ExPresentEx(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

void OnDirect3DDeviceCreated(IDirect3DDevice9* device, bool ex)
{
	if (state_flags & 2)
		return;

	state_flags |= 2;

	g_loader->update_info.device = device;
	INSTALL_VTABLE_HOOK(IDirect3DDevice9, device, Present);
	if (ex)
	{
		INSTALL_VTABLE_HOOK(IDirect3DDevice9Ex, device, PresentEx);
	}
}

VTABLE_HOOK(HRESULT, WINAPI, IDirect3D9, CreateDevice, 16, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
	const auto result = originalIDirect3D9CreateDevice(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (*ppReturnedDeviceInterface)
	{
		OnDirect3DDeviceCreated(*ppReturnedDeviceInterface, false);
	}

	return result;
}

VTABLE_HOOK(HRESULT, WINAPI, IDirect3D9, CreateDeviceEx, 20, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface)
{
	const auto result = originalIDirect3D9CreateDeviceEx(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (*ppReturnedDeviceInterface)
	{
		OnDirect3DDeviceCreated(*ppReturnedDeviceInterface, true);
	}

	return result;
}

void OnDirect3DCreated(IDirect3D9* d3d, bool ex)
{
	if (state_flags & 1)
		return;

	state_flags |= 1;
	if (d3d == nullptr)
	{
		LOG("Direct3DCreate9() returned nullptr");
		return;
	}

	INSTALL_VTABLE_HOOK(IDirect3D9, d3d, CreateDevice);
	if (ex)
	{
		INSTALL_VTABLE_HOOK(IDirect3D9, d3d, CreateDeviceEx);
	}
}

HOOK(IDirect3D9*, WINAPI, Direct3DCreate9Hook, PROC_ADDRESS("d3d9", "Direct3DCreate9"), UINT SDK_VERSION)
{
	auto* d3d = originalDirect3DCreate9Hook(SDK_VERSION);
	OnDirect3DCreated(d3d, false);
	return d3d;
}

HOOK(HRESULT, WINAPI, Direct3DCreate9ExHook, PROC_ADDRESS("d3d9", "Direct3DCreate9Ex"), UINT SDK_VERSION, IDirect3D9Ex** ppD3d)
{
	const auto hr = originalDirect3DCreate9ExHook(SDK_VERSION, ppD3d);
	OnDirect3DCreated(*ppD3d, true);
	return hr;
}

void D3D9Hooks_Init()
{
	INSTALL_HOOK(Direct3DCreate9Hook);
	INSTALL_HOOK(Direct3DCreate9ExHook);
}