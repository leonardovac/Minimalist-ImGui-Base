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

    inline TinyHook::VMTHook* deviceHook;

    static bool Init()
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

#if USE_VMTHOOK_WHEN_AVAILABLE 
        deviceHook = new HookFramework::VMTHook(pVTable);
    	deviceHook->Hook(16, &Reset);
    	deviceHook->Hook(17, &Present);
#else
        HooksManager::Setup<InlineHook>(pVTable[16], FUNCTION(Reset));
        HooksManager::Setup<InlineHook>(pVTable[17], FUNCTION(Present));
#endif
        return true;
    }

    inline void CleanupDeviceD3D()
	{
		std::thread([]
		{
#if USE_VMTHOOK_WHEN_AVAILABLE
			deviceHook.reset();
#else
			HooksManager::Unhook(&Reset, &Present);
#endif

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

    inline HRESULT WINAPI Present(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, const HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, const DWORD dwFlags)
    {
	    [&pDevice]
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
	    }();
	    static const OriginalFunc originalFunction(&Present);
	    return originalFunction.stdcall<HRESULT>(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
    }
}
