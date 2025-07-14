#pragma once
#ifdef _WIN64
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <windows.h>
#include <Mem/mem.h>

#include "../menu.h"
#include "../overlay.h"

namespace Overlay::DirectX12
{
	void ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT numCommandLists, ID3D12CommandList* ppCommandLists);
	HRESULT ResizeBuffers(IDXGISwapChain3* pSwapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
	HRESULT Present(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT uFlags);

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

	inline bool Init()
	{
		// RAII wrapper to ensure window is deleted on scope exit
		WinGuard window;
		if (!window) return false;

		const HMODULE hD3D12 = GetModuleHandleW(L"d3d12.dll");
		const HMODULE hDXGI = GetModuleHandleW(L"dxgi.dll");
		if (!hD3D12 || !hDXGI) return false;

		void* CreateDXGIFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
		if (CreateDXGIFactory == nullptr) return false;

		IDXGIFactory* pFactory;
		if (FAILED(static_cast<long(*)(const IID&, void**)>(CreateDXGIFactory)(IID_PPV_ARGS(&pFactory)))) return false;

		IDXGIAdapter* pAdapter;
		if (pFactory->EnumAdapters(0, &pAdapter) == DXGI_ERROR_NOT_FOUND) return false;

		void* D3D12CreateDevice = GetProcAddress(hD3D12, "D3D12CreateDevice");
		if (D3D12CreateDevice == nullptr) return false;

		ID3D12Device* pDevice;
		if (FAILED(static_cast<long(*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**)>(D3D12CreateDevice)(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)))) return false;

		D3D12_COMMAND_QUEUE_DESC queueDesc;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = 0;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		ComPtr<ID3D12CommandQueue> pCommandQueue;
		if (FAILED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)))) return false;

		ComPtr<ID3D12CommandAllocator> pCommandAllocator;
		if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator)))) return false;

		ComPtr<ID3D12GraphicsCommandList> pCommandList;
		if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pCommandList)))) return false;

		DXGI_RATIONAL refreshRate;
		refreshRate.Numerator = 60;
		refreshRate.Denominator = 1;

		DXGI_MODE_DESC bufferDesc;
		bufferDesc.Width = 100;
		bufferDesc.Height = 100;
		bufferDesc.RefreshRate = refreshRate;
		bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC sampleDesc;
		sampleDesc.Count = 1;
		sampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		swapChainDesc.BufferDesc = bufferDesc;
		swapChainDesc.SampleDesc = sampleDesc;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.OutputWindow = hWindow;
		swapChainDesc.Windowed = (GetWindowLongPtr(hWindow, GWL_STYLE) & WS_POPUP) == 0;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ComPtr<IDXGISwapChain> pSwapChain;
		if (FAILED(pFactory->CreateSwapChain(pCommandQueue.Get(), &swapChainDesc, &pSwapChain))) return false;

		uintptr_t** pvCommandQueue = *reinterpret_cast<uintptr_t***>(pCommandQueue.Get());
		uintptr_t** pvSwapChain = *reinterpret_cast<uintptr_t***>(pSwapChain.Get());

		HooksManager::Setup<InlineHook>(pvCommandQueue[10], FUNCTION(ExecuteCommandLists));
		HooksManager::Setup<InlineHook>(pvSwapChain[8], FUNCTION(Present));
		HooksManager::Setup<InlineHook>(pvSwapChain[13], FUNCTION(ResizeBuffers));
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
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		ReleaseMainTargetView();
		SafeRelease(Interface::pSwapChain);
		SafeRelease(Interface::pCommandAllocator);
		SafeRelease(Interface::pCommandQueue);
		SafeRelease(Interface::pCommandList);
		SafeRelease(Interface::pDescHeapBackBuffers);
		SafeRelease(Interface::pDescHeapImGuiRender);
		SafeRelease(Interface::pDevice);
	}

	inline HRESULT Present(IDXGISwapChain3* pSwapChain, const UINT SyncInterval, const UINT uFlags)
	{
		Interface::pSwapChain = pSwapChain;
		[&pSwapChain]
		{
			if (!Overlay::bInitialized)
			{
				if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&Overlay::DirectX12::Interface::pDevice))))
				{
					DXGI_SWAP_CHAIN_DESC descSwapChain;
					if (SUCCEEDED(pSwapChain->GetDesc(&descSwapChain)))
					{
						lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(descSwapChain.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

						Interface::nBuffersCounts = descSwapChain.BufferCount;
						Interface::pFrameContext = new Interface::FrameContext[Interface::nBuffersCounts];

						D3D12_DESCRIPTOR_HEAP_DESC descBackBuffers;
						descBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
						descBackBuffers.NumDescriptors = Interface::nBuffersCounts;
						descBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
						descBackBuffers.NodeMask = 1;

						if (FAILED(Interface::pDevice->CreateDescriptorHeap(&descBackBuffers, IID_PPV_ARGS(&Interface::pDescHeapBackBuffers)))) return;

						D3D12_DESCRIPTOR_HEAP_DESC descImGuiRender = {};
						descImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
						descImGuiRender.NumDescriptors = Interface::nBuffersCounts;
						descImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

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
						ImGui::CreateContext();
						Menu::SetupImGuiStyle();
						// Setup Platform/Renderer backends
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

						if (!((Overlay::bInitialized = ImGui_ImplDX12_Init(&initInfo)))) return;
						ImGui_ImplDX12_CreateDeviceObjects();
					}
				}
			}

			if (!bEnabled || Interface::pCommandQueue == nullptr) return;

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
			Interface::pCommandList->Reset(Interface::pCommandAllocator, nullptr);
			Interface::pCommandList->ResourceBarrier(1, &barrier);

			Interface::pCommandList->OMSetRenderTargets(1, &Interface::pFrameContext[backBufferIndex].pDescriptorHandle, FALSE, nullptr);
			Interface::pCommandList->SetDescriptorHeaps(1, &Interface::pDescHeapImGuiRender);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Interface::pCommandList);
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			Interface::pCommandList->ResourceBarrier(1, &barrier);
			Interface::pCommandList->Close();
			Interface::pCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&Interface::pCommandList));
		}();
		return HooksManager::GetOriginal(&Present).unsafe_call<long>(pSwapChain, SyncInterval, uFlags);
	}

	inline void ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, const UINT numCommandLists, ID3D12CommandList* ppCommandLists)
	{
		if (!Interface::pCommandQueue) Interface::pCommandQueue = pCommandQueue;
		return HooksManager::GetOriginal(&ExecuteCommandLists).unsafe_call<void>(pCommandQueue, numCommandLists, ppCommandLists);
	}

	inline HRESULT ResizeBuffers(IDXGISwapChain3* pSwapChain, const UINT bufferCount, const UINT width, const UINT height, const DXGI_FORMAT newFormat, const UINT swapChainFlags)
	{
		ReleaseMainTargetView();
		const HRESULT result = HooksManager::GetOriginal(&ResizeBuffers).unsafe_call<HRESULT>(pSwapChain, bufferCount, width, height, newFormat, swapChainFlags);
		CreateMainTargetView();
		return result;
	}
}
#endif