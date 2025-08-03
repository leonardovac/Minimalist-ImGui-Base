#pragma once
#include <d3d9.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "../menu.h"
#include "../overlay.h"
#include "../../hooks.h"

namespace Overlay::DirectX9
{
    HRESULT WINAPI Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, DWORD dwFlags);
	HRESULT WINAPI Present(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags);
	HRESULT WINAPI SwapChainPresent(IDirect3DSwapChain9* pSwapChain, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags);
	HRESULT WINAPI EndScene(IDirect3DDevice9* pDevice);

	inline bool Hook()
	{
        // RAII wrapper to ensure window is deleted on scope exit
        const WinGuard window;
        if (!window) return false;

        const HMODULE hD3D9 = GetModuleHandleW(L"d3d9.dll");
        if (!hD3D9) return false;

        void* Direct3DCreate9 = GetProcAddress(hD3D9, "Direct3DCreate9");
        if (!Direct3DCreate9) return false;

    	const ComPtr<IDirect3D9> pD3D = static_cast<IDirect3D9*(*)(UINT)>(Direct3DCreate9)(D3D_SDK_VERSION);
        if (!pD3D) return false;

        D3DPRESENT_PARAMETERS parameters = {};
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow = window.handle;
        parameters.Windowed = (GetWindowLongPtr(window.handle, GWL_STYLE) & WS_POPUP) == 0;
       

        ComPtr<IDirect3DDevice9> pDevice;
        if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, window.handle, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &pDevice))) return false;

        void** pVTable = *reinterpret_cast<void***>(pDevice.Get());

        HooksManager::Create<InlineHook>(pVTable[16], PTR_AND_NAME(Reset));
		HooksManager::Create<InlineHook>(pVTable[42], PTR_AND_NAME(EndScene));
        return true;
    }

    inline void CleanupDeviceD3D()
	{
		std::thread([]
		{
			HooksManager::Unhook(&Reset, &Present);

			DisableRendering();

			ImGui_ImplDX9_Shutdown();
			Menu::CleanupImGui();
		}).detach();
	}

    inline HRESULT WINAPI Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, const DWORD dwFlags)
    {
	    ImGui_ImplDX9_InvalidateDeviceObjects();

        static const OriginalFunc originalFunction(&Reset);
		const auto& result = originalFunction.stdcall<HRESULT>(pDevice, pPresentationParameters, dwFlags);
    
		ImGui_ImplDX9_CreateDeviceObjects();
        return result;
    }

	inline void RenderOverlay(IDirect3DDevice9* pDevice)
    {
		if (!Overlay::bEnabled)
		{
			SetEvent(screenCleaner.eventPresentSkipped);
			return;
		}

		if (!Overlay::bInitialized)
		{
			// Setup Dear ImGui context
			Menu::SetupImGui();
			// Setup Platform/Renderer backends 
			if (!ImGui_ImplWin32_Init(hWindow) || !ImGui_ImplDX9_Init(pDevice)) return;
			Overlay::bInitialized = true;
		}

		if (!ImGui::GetIO().BackendRendererUserData) return;

		// ImGui expects linear color space (D3DFMT_A8R8G8B8), e.g:
		// https://stackoverflow.com/questions/69327444/the-data-rendered-of-imgui-within-window-get-a-lighter-color
		DWORD oldValue;
		(void)pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &oldValue);
		(void)pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();

		Overlay::RenderLogic();

		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		(void)pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, oldValue);
    }

    inline HRESULT WINAPI EndScene(IDirect3DDevice9* pDevice)
    {
		RenderOverlay(pDevice);
	    static const OriginalFunc originalFunction(&EndScene);
	    return originalFunction.stdcall<HRESULT>(pDevice);
    }

    inline HRESULT WINAPI Present(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, const HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, const DWORD dwFlags)
    {
		RenderOverlay(pDevice);
	    static const OriginalFunc originalFunction(&Present);
	    return originalFunction.stdcall<HRESULT>(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
    }

	inline HRESULT WINAPI SwapChainPresent(IDirect3DSwapChain9* pSwapChain, const RECT* pSourceRect, const RECT* pDestRect, const HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, const DWORD dwFlags)
	{
		IDirect3DDevice9* pDevice {nullptr};
		if (SUCCEEDED(pSwapChain->GetDevice(&pDevice)))
		{
			RenderOverlay(pDevice);
		}

		static const OriginalFunc originalFunction(&SwapChainPresent);
		return originalFunction.stdcall<HRESULT>(pSwapChain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
	}
}
