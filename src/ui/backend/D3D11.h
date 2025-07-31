#pragma once
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "../overlay.h"
#include "../../hooks.h"


namespace Overlay::DirectX11
{
	HRESULT WINAPI PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT uFlags);
	HRESULT WINAPI ResizeBuffersHook(IDXGISwapChain* pSwapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);

	inline std::unique_ptr<TinyHook::VMTHook> swapChainHook;

	namespace Interface
	{
		typedef HRESULT(WINAPI* D3D11CREATEDEVICEANDSWAPCHAIN)
			(
				IDXGIAdapter* pAdapter,
				D3D_DRIVER_TYPE DriverType,
				HMODULE Software,
				UINT Flags,
				const D3D_FEATURE_LEVEL* pFeatureLevels,
				UINT FeatureLevels,
				UINT SDKVersion,
				const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
				IDXGISwapChain** ppSwapChain,
				ID3D11Device** ppDevice,
				D3D_FEATURE_LEVEL* pFeatureLevel,
				ID3D11DeviceContext** ppImmediateContext
				);

		inline ID3D11Device* pDevice = nullptr;
		inline ID3D11DeviceContext* pDeviceContext = nullptr;
		inline ID3D11RenderTargetView* pRenderTargetView = nullptr;
	}

	inline bool Hook()
	{
		// RAII wrapper to ensure window is deleted on scope exit
		WinGuard window;
		if (!window) return false;

		const HMODULE hD3D11 = GetModuleHandleW(L"d3d11.dll");
		if (!hD3D11) return false;

		const auto D3D11CreateDeviceAndSwapChain = reinterpret_cast<Interface::D3D11CREATEDEVICEANDSWAPCHAIN>(GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain"));
		if (!D3D11CreateDeviceAndSwapChain) return false;

		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 2;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.OutputWindow = window.handle;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = (GetWindowLongPtr(window.handle, GWL_STYLE) & WS_POPUP) == 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		ComPtr<IDXGISwapChain> pSwapChain;
		ComPtr<ID3D11Device> pDevice;
		constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, std::size(featureLevels), D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, nullptr, nullptr))) return false;

		uintptr_t** pVTable = *reinterpret_cast<uintptr_t***>(pSwapChain.Get());

#if USE_VMTHOOK_WHEN_AVAILABLE 
		swapChainHook = std::make_unique<TinyHook::VMTHook>(pVTable);
		swapChainHook->Hook(8, &PresentHook);
		swapChainHook->Hook(13, &ResizeBuffersHook);
#else
		HooksManager::Setup<InlineHook>(pVTable[8], PTR_AND_NAME(PresentHook));
		HooksManager::Setup<InlineHook>(pVTable[13], PTR_AND_NAME(ResizeBuffersHook));
#endif
		return true;
	}

	inline void CreateMainRenderTargetView(IDXGISwapChain* pSwapChain)
	{
		ComPtr<ID3D11Texture2D> pBackBuffer;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
		{
			(void)Interface::pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &Interface::pRenderTargetView);
		}
	}

	inline void ReleaseRenderTargetView()
	{
		SafeRelease(Interface::pRenderTargetView);
	}

	inline void CleanupDeviceD3D()
	{
		std::thread([]
		{
#if USE_VMTHOOK_WHEN_AVAILABLE
			swapChainHook.reset();
#else
			HooksManager::Unhook(&PresentHook, &ResizeBuffersHook);
#endif

			DisableRendering();

			ImGui_ImplDX11_Shutdown();
			Menu::CleanupImGui();

			ReleaseRenderTargetView();
			SafeRelease(Interface::pDevice);
			SafeRelease(Interface::pDeviceContext);
		}).detach();
	}

	inline HRESULT WINAPI PresentHook(IDXGISwapChain* pSwapChain, const UINT SyncInterval, const UINT uFlags)
	{
		[&pSwapChain]
		{
			if (!Overlay::bEnabled)
			{
				SetEvent(screenCleaner.eventPresentSkipped);
				return;
			}

			if (!Overlay::bInitialized)
			{
				if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&Overlay::DirectX11::Interface::pDevice)))) return;
				Interface::pDevice->GetImmediateContext(&Interface::pDeviceContext);

				// Setup Dear ImGui context 
				Menu::SetupImGui();
				// Setup Platform/Renderer backends 
				if (!ImGui_ImplWin32_Init(hWindow) || !ImGui_ImplDX11_Init(Interface::pDevice, Interface::pDeviceContext)) return;
				CreateMainRenderTargetView(pSwapChain);
				Overlay::bInitialized = true;
			}

			// Draw ImGui
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			Interface::pDeviceContext->OMSetRenderTargets(1, &Interface::pRenderTargetView, nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}();

		static const OriginalFunc originalFunction(&PresentHook);
		return originalFunction.stdcall<HRESULT>(pSwapChain, SyncInterval, uFlags);
	}

	inline HRESULT WINAPI ResizeBuffersHook(IDXGISwapChain* pSwapChain, const UINT bufferCount, const UINT width, const UINT height, const DXGI_FORMAT newFormat, const UINT swapChainFlags)
	{
		ReleaseRenderTargetView();
		static const OriginalFunc originalFunction(&ResizeBuffersHook);
		const HRESULT result = originalFunction.stdcall<HRESULT>(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
		CreateMainRenderTargetView(pSwapChain);
		return result;
	}
}
