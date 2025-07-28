#pragma once
#include <WinSDKVer.h>
#if defined(_WIN64) && (_WIN32_WINNT >= _WIN32_WINNT_WIN10) // DirectX 12 is supported on Windows 10 and later operating systems.
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "../menu.h"
#include "../overlay.h"

namespace Overlay::DirectX12
{
	void ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT numCommandLists, ID3D12CommandList* ppCommandLists);
	HRESULT ResizeBuffers(IDXGISwapChain3* pSwapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
	HRESULT Present(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT uFlags);

	inline std::unique_ptr<TinyHook::VMTHook> commandQueueHook;
	inline std::unique_ptr<TinyHook::VMTHook> swapChainHook;

	namespace Interface
	{
		inline ID3D12CommandAllocator* pCommandAllocator;
		inline ID3D12GraphicsCommandList* pCommandList;
		inline ID3D12CommandQueue* pCommandQueue;
		inline ID3D12Device* pDevice;
		inline ID3D12DescriptorHeap* pDescHeapBackBuffers;
		inline ID3D12DescriptorHeap* pDescHeapImGuiRender;

		inline IDXGISwapChain3* pSwapChain;

		struct HeapAllocator
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHeapStart;
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart;
			UINT incrementSize;
			std::vector<UINT> freeIndices;

			void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
			{
				assert(heap != nullptr && freeIndices.empty());
				const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
				cpuHeapStart = heap->GetCPUDescriptorHandleForHeapStart();
				gpuHeapStart = heap->GetGPUDescriptorHandleForHeapStart();
				incrementSize = device->GetDescriptorHandleIncrementSize(desc.Type);
				freeIndices.reserve(desc.NumDescriptors);
				for (UINT n = desc.NumDescriptors; n > 0; n--)
				{
					freeIndices.push_back(n - 1);
				}
			}

			void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* pCpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pGpuDescriptor)
			{
				assert(!freeIndices.empty());
				const SIZE_T index = freeIndices.back();
				freeIndices.pop_back();
				pCpuDescriptor->ptr = cpuHeapStart.ptr + index * incrementSize;
				pGpuDescriptor->ptr = gpuHeapStart.ptr + index * incrementSize;
			}

			void Free(const D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, const D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor)
			{
				const UINT cpuIndex = static_cast<UINT>((cpuDescriptor.ptr - cpuHeapStart.ptr) / incrementSize);
				[[maybe_unused]] const UINT gpuIndex = static_cast<UINT>((gpuDescriptor.ptr - gpuHeapStart.ptr) / incrementSize);
				assert(cpuIndex == gpuIndex);
				freeIndices.push_back(cpuIndex);
			}
		}; inline HeapAllocator heapAllocator;

		struct FrameContext
		{
			D3D12_CPU_DESCRIPTOR_HANDLE		pDescriptorHandle;
			ID3D12Resource* pResource;
		}; inline FrameContext* pFrameContext;

		inline UINT	nBuffersCounts = -1;
	}

	inline bool CreateFactoryAndCommandQueue(ComPtr<IDXGIFactory>& pFactory, ComPtr<ID3D12CommandQueue>& pCommandQueue)
	{
		const HMODULE hD3D12 = GetModuleHandleW(L"d3d12.dll");
		const HMODULE hDXGI = GetModuleHandleW(L"dxgi.dll");
		if (!hD3D12 || !hDXGI) return false;

		void* CreateDXGIFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
		if (CreateDXGIFactory == nullptr) return false;

		if (FAILED(static_cast<long(*)(const IID&, void**)>(CreateDXGIFactory)(IID_PPV_ARGS(&pFactory)))) return false;

		ComPtr<IDXGIAdapter> pAdapter;
		if (FAILED(pFactory->EnumAdapters(0, &pAdapter))) return false;

		void* D3D12CreateDevice = GetProcAddress(hD3D12, "D3D12CreateDevice");
		if (D3D12CreateDevice == nullptr) return false;

		ComPtr<ID3D12Device> pDevice;
		if (FAILED(static_cast<long(*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**)>(D3D12CreateDevice)(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)))) return false;

		constexpr D3D12_COMMAND_QUEUE_DESC queueDesc{ D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE , 0 };
		
		if (FAILED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)))) return false;

		uintptr_t** pvCommandQueue = *reinterpret_cast<uintptr_t***>(pCommandQueue.Get());

#if USE_VMTHOOK_WHEN_AVAILABLE
		commandQueueHook = std::make_unique<HookFramework::VMTHook>(pvCommandQueue);
		commandQueueHook->Hook(10, &ExecuteCommandLists);
#else
		HooksManager::Setup<InlineHook>(pvCommandQueue[10], FUNCTION(ExecuteCommandLists));
#endif
		return true;
	}

	inline bool CreateFactoryAndCommandQueue()
	{
		ComPtr<IDXGIFactory> pFactory;
		ComPtr<ID3D12CommandQueue> pCommandQueue;
		return CreateFactoryAndCommandQueue(pFactory, pCommandQueue);
	}
	inline bool Init()
	{
		// RAII wrapper to ensure window is deleted on scope exit
		const WinGuard window;
		if (!window) return false;

		ComPtr<IDXGIFactory> pFactory;
		ComPtr<ID3D12CommandQueue> pCommandQueue;
		if (!CreateFactoryAndCommandQueue(pFactory, pCommandQueue)) return false;

		constexpr DXGI_RATIONAL refreshRate {60, 1};
		constexpr DXGI_MODE_DESC bufferDesc {100, 100, refreshRate, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED };
		constexpr DXGI_SAMPLE_DESC sampleDesc {1, 0};

		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		swapChainDesc.BufferDesc = bufferDesc;
		swapChainDesc.SampleDesc = sampleDesc;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.OutputWindow = window.handle;
		swapChainDesc.Windowed = (GetWindowLongPtr(window.handle, GWL_STYLE) & WS_POPUP) == 0;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ComPtr<IDXGISwapChain> pSwapChain;
		if (FAILED(pFactory->CreateSwapChain(pCommandQueue.Get(), &swapChainDesc, &pSwapChain))) return false;

		uintptr_t** pvSwapChain = *reinterpret_cast<uintptr_t***>(pSwapChain.Get());

#if USE_VMTHOOK_WHEN_AVAILABLE
		swapChainHook = std::make_unique<HookFramework::VMTHook>(pvSwapChain);
		swapChainHook->Hook(8, &Present);
		swapChainHook->Hook(13, &ResizeBuffers);
#else
		HooksManager::Setup<InlineHook>(pvSwapChain[8], FUNCTION(Present));
		HooksManager::Setup<InlineHook>(pvSwapChain[13], FUNCTION(ResizeBuffers));
#endif
		return true;
	}

	inline void CreateMainTargetView()
	{
		for (UINT i = 0; i < Interface::nBuffersCounts; i++)
		{
			if (SUCCEEDED(Interface::pSwapChain->GetBuffer(i, IID_PPV_ARGS(&Interface::pFrameContext[i].pResource))))
			{
				Interface::pDevice->CreateRenderTargetView(Interface::pFrameContext[i].pResource, nullptr, Interface::pFrameContext[i].pDescriptorHandle);
			}
		}
	}

	inline void ReleaseMainTargetView()
	{
		for (UINT i = 0; i < Interface::nBuffersCounts; i++)
		{
			SafeRelease(Interface::pFrameContext[i].pResource);
		}
	}

	inline void CleanupDeviceD3D()
	{
		std::thread([]
		{
#if USE_VMTHOOK_WHEN_AVAILABLE
			commandQueueHook.reset();
			swapChainHook.reset();
#else
			HooksManager::Unhook(&Present);
			HooksManager::Unhook(&ResizeBuffers);
#endif

			DisableRendering();

			ImGui_ImplDX12_Shutdown();
			Menu::CleanupImGui();

			ReleaseMainTargetView();
			SafeRelease(Interface::pSwapChain);
			SafeRelease(Interface::pCommandQueue);
			SafeRelease(Interface::pDescHeapBackBuffers);
			SafeRelease(Interface::pDescHeapImGuiRender);
			SafeRelease(Interface::pDevice);
		}).detach();
	}

	inline HRESULT Present(IDXGISwapChain3* pSwapChain, const UINT SyncInterval, const UINT uFlags)
	{
		[&pSwapChain]
		{
			Interface::pSwapChain = pSwapChain;
			if (!Overlay::bInitialized)
			{
				if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&Overlay::DirectX12::Interface::pDevice)))) return;
			
				DXGI_SWAP_CHAIN_DESC descSwapChain;
				if (FAILED(pSwapChain->GetDesc(&descSwapChain))) return;
				
				hWindow = descSwapChain.OutputWindow;
				lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

				Interface::nBuffersCounts = descSwapChain.BufferCount;
				Interface::pFrameContext = new Interface::FrameContext[Interface::nBuffersCounts];

				const D3D12_DESCRIPTOR_HEAP_DESC descBackBuffers { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Interface::nBuffersCounts, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1 };
				if (FAILED(Interface::pDevice->CreateDescriptorHeap(&descBackBuffers, IID_PPV_ARGS(&Interface::pDescHeapBackBuffers)))) return;

				const D3D12_DESCRIPTOR_HEAP_DESC descImGuiRender = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Interface::nBuffersCounts, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1};
				if (FAILED(Interface::pDevice->CreateDescriptorHeap(&descImGuiRender, IID_PPV_ARGS(&Interface::pDescHeapImGuiRender)))) return;

				if (FAILED(Interface::pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Interface::pCommandAllocator)))) return;

				if (FAILED(Interface::pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Interface::pCommandAllocator, nullptr, IID_PPV_ARGS(&Interface::pCommandList))) || FAILED(Interface::pCommandList->Close())) return;

				const auto rtvDescriptorSize = Interface::pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = Interface::pDescHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();
				for (UINT i = 0; i < Interface::nBuffersCounts; i++)
				{
					Interface::pFrameContext[i].pDescriptorHandle = rtvHandle;
					rtvHandle.ptr += rtvDescriptorSize;
				}

				CreateMainTargetView();

				// Setup Dear ImGui context
				Menu::SetupImGui();
				// Setup Platform backend
				if (!ImGui_ImplWin32_Init(descSwapChain.OutputWindow)) return;

				ImGui_ImplDX12_InitInfo initInfo = {};
				initInfo.Device = Interface::pDevice;
				initInfo.CommandQueue = Interface::pCommandQueue;
				initInfo.NumFramesInFlight = static_cast<int>(Interface::nBuffersCounts);
				initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
				initInfo.SrvDescriptorHeap = Interface::pDescHeapImGuiRender;

				Interface::heapAllocator.Create(Interface::pDevice, Interface::pDescHeapImGuiRender);

				initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* phCpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE* phGpuDesc)
				{
					return Interface::heapAllocator.Alloc(phCpuDesc, phGpuDesc);
				};

				initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, const D3D12_CPU_DESCRIPTOR_HANDLE hCpuHeapCurrent, const D3D12_GPU_DESCRIPTOR_HANDLE hGpuHeapCurrent)
				{
					return Interface::heapAllocator.Free(hCpuHeapCurrent, hGpuHeapCurrent);
				};
				// Setup Renderer backend
				if (!ImGui_ImplDX12_Init(&initInfo)) return;
				Overlay::bInitialized = true;
			}

			if (!Overlay::bEnabled)
			{
				SetEvent(screenCleaner.eventPresentSkipped);
				return;
			}

			if (!Interface::pCommandQueue) return;

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			const UINT backBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = Interface::pFrameContext[backBufferIndex].pResource;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			if(FAILED(Interface::pCommandList->Reset(Interface::pCommandAllocator, nullptr))) return;
			Interface::pCommandList->ResourceBarrier(1, &barrier);
			Interface::pCommandList->OMSetRenderTargets(1, &Interface::pFrameContext[backBufferIndex].pDescriptorHandle, FALSE, nullptr);
			Interface::pCommandList->SetDescriptorHeaps(1, &Interface::pDescHeapImGuiRender);

			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Interface::pCommandList);
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			Interface::pCommandList->ResourceBarrier(1, &barrier);

			if (FAILED(Interface::pCommandList->Close())) return;
			Interface::pCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&Interface::pCommandList));
		}();

		static const OriginalFunc originalFunction(&Present);
		return originalFunction.stdcall<HRESULT>(pSwapChain, SyncInterval, uFlags);
	}

	inline void ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, const UINT numCommandLists, ID3D12CommandList* ppCommandLists)
	{
		Interface::pCommandQueue = pCommandQueue;
		static const OriginalFunc originalFunction(&ExecuteCommandLists);
		return originalFunction.stdcall<void>(pCommandQueue, numCommandLists, ppCommandLists);
	}

	inline HRESULT ResizeBuffers(IDXGISwapChain3* pSwapChain, const UINT bufferCount, const UINT width, const UINT height, const DXGI_FORMAT newFormat, const UINT swapChainFlags)
	{
		ReleaseMainTargetView();
		static const OriginalFunc originalFunction(&ResizeBuffers);
		const HRESULT result = originalFunction.stdcall<HRESULT>(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
		CreateMainTargetView();
		return result;
	}
}
#endif