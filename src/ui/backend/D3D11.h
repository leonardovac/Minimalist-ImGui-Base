#pragma once
#include <d3d11.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>


namespace Overlay::DirectX11
{
	long PresentHook(IDXGISwapChain3* pSwapChain, const UINT SyncInterval, const UINT uFlags);
	HRESULT ResizeBuffersHook(IDXGISwapChain3* pSwapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);

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
		inline IDXGISwapChain* pSwapChain = nullptr;
		inline ID3D11RenderTargetView* pRenderTargetView = nullptr;
	}

	inline bool Init()
	{
		// RAII wrapper to ensure DeleteWindow() is called on scope exit
		struct Guard {
			~Guard() { DeleteWindow(); }
		} guard;

		const HMODULE hD3D11 = GetModuleHandleW(L"d3d11.dll");
		if (!hD3D11) return false;

		const auto D3D11CreateDeviceAndSwapChain = reinterpret_cast<Interface::D3D11CREATEDEVICEANDSWAPCHAIN>(GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain"));
		if (!D3D11CreateDeviceAndSwapChain) return false;

		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 2;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.OutputWindow = hWindow;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = (GetWindowLongPtr(hWindow, GWL_STYLE) & WS_POPUP) == 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		ComPtr<IDXGISwapChain> pSwapChain;
		ComPtr<ID3D11Device> pDevice;
		constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, std::size(featureLevels), D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, nullptr, nullptr))) return false;

		uintptr_t** pVTable = *reinterpret_cast<uintptr_t***>(pSwapChain.Get());

		HooksManager::Setup<InlineHook>(pVTable[8], &PresentHook);
		HooksManager::Setup<InlineHook>(pVTable[13], &ResizeBuffersHook);
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
		if (Interface::pRenderTargetView) { Interface::pRenderTargetView->Release(); Interface::pRenderTargetView = nullptr; }
	}

	inline void CleanupDeviceD3D()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		ReleaseRenderTargetView();
		if (Interface::pSwapChain) { Interface::pSwapChain->Release(); Interface::pSwapChain = nullptr; }
		if (Interface::pDeviceContext) { Interface::pDeviceContext->Release(); Interface::pDeviceContext = nullptr; }
		if (Interface::pDevice) { Interface::pDevice->Release(); Interface::pDevice = nullptr; }
	}

	inline long PresentHook(IDXGISwapChain3* pSwapChain, const UINT SyncInterval, const UINT uFlags)
	{
		Interface::pSwapChain = pSwapChain;
		[&pSwapChain]
		{
			if (!Overlay::bInitialized)
			{
				if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&Overlay::DirectX11::Interface::pDevice))))
				{
					Interface::pDevice->GetImmediateContext(&Interface::pDeviceContext);
					DXGI_SWAP_CHAIN_DESC sd;
					if (SUCCEEDED(pSwapChain->GetDesc(&sd)))
					{
						hWindow = sd.OutputWindow;
						lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
						// Setup Dear ImGui context 
						ImGui::CreateContext();
						Menu::SetupImGuiStyle();
						// Setup Platform/Renderer backends 
						if (!ImGui_ImplWin32_Init(hWindow) || !((Overlay::bInitialized = ImGui_ImplDX11_Init(Interface::pDevice, Interface::pDeviceContext)))) return;
						CreateMainRenderTargetView(pSwapChain);
					}
				}
			}

			if (!bEnabled) return;

			// Draw ImGui
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			Interface::pDeviceContext->OMSetRenderTargets(1, &Interface::pRenderTargetView, nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}();
		return HooksManager::GetOriginal(&PresentHook).unsafe_stdcall<long>(pSwapChain, SyncInterval, uFlags);
	}

	inline HRESULT ResizeBuffersHook(IDXGISwapChain3* pSwapChain, const UINT bufferCount, const UINT width, const UINT height, const DXGI_FORMAT newFormat, const UINT swapChainFlags)
	{
		ReleaseRenderTargetView();
		const HRESULT result = HooksManager::GetOriginal(&ResizeBuffersHook).unsafe_stdcall<HRESULT>(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
		CreateMainRenderTargetView(pSwapChain);
		return result;
	}
}
