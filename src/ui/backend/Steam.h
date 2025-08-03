#pragma once
#include <windows.h>

#include "Vulkan.h"
#include "../overlay.h"
#include "Mem/mem.h"
#include "TinyHook/tinyhook.h"

namespace Overlay::Steam
{

	static void SwapPointer(uintptr_t* pAddress, const void* newAddress, const char* functionName = "Unknown")
	{
		if (!pAddress || !newAddress)
		{
			LOG_ERROR("Invalid pointer passed, pAddress: {} | newAddress: {} ({})", reinterpret_cast<uintptr_t>(pAddress), reinterpret_cast<uintptr_t>(newAddress), functionName);
			return;
		}
		const uintptr_t pOriginal = *pAddress;
		// Map the original function to our detour, so we call use GetOriginal
		TinyHook::Manager::RegisterHook(newAddress, pOriginal);
		*pAddress = reinterpret_cast<uintptr_t>(newAddress);
		LOG_INFO("Swapped pointer at 0x{:X} from 0x{:X} to 0x{:X} ({})", reinterpret_cast<uintptr_t>(pAddress), pOriginal, reinterpret_cast<uintptr_t>(newAddress), functionName);
	}

#ifdef _WIN64
	constexpr auto game_overlay_renderer = "GameOverlayRenderer64.dll";
	constexpr auto steam_overlay_vulkan_layer = "SteamOverlayVulkanLayer64.dll";
	constexpr auto d3d_present_pattern = "48 8B 05 ?? ?? ?? ?? 44 8B C5 8B D6";
	constexpr auto d3d9_present_pattern = "48 8B 05 ?? ?? ?? ?? 4C 8B C5 49 8B D7";
	constexpr auto d3d9_swapchain_present_pattern = "4C 8B 15 ?? ?? ?? ?? 4C 8B C5";
	constexpr auto opengl_swap_buffers_pattern = "48 8B 05 ?? ?? ?? ?? 48 8B CB FF D0 80 3D";

	static uintptr_t* GetPointerFromRef(const uintptr_t instructionStart)
	{
		return reinterpret_cast<uintptr_t*>(instructionStart + 7 + *reinterpret_cast<int32_t*>(instructionStart + 3));
	}
#else
	constexpr auto game_overlay_renderer = "GameOverlayRenderer.dll";
	constexpr auto steam_overlay_vulkan_layer = "SteamOverlayVulkanLayer.dll";
	constexpr auto d3d_present_pattern = "A1 ?? ?? ?? ?? 53 FF 75 ?? FF 75 ?? FF D0 8B 4D ?? 64 89 0D ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C2 ?? ?? 68 ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 75";
	constexpr auto d3d9_present_pattern = "A1 ?? ?? ?? ?? 51 53 FF D0";
	constexpr auto d3d9_swapchain_present_pattern = "A1 ?? ?? ?? ?? 56 FF 75 ?? FF 75 ?? FF 75 ?? FF 75 ?? 57";
	constexpr auto opengl_swap_buffers_pattern = "A1 ?? ?? ?? ?? 56 FF D0 80 3D";

	static uintptr_t* GetPointerFromRef(const uintptr_t instructionStart)
	{
		return *reinterpret_cast<uintptr_t**>(instructionStart + 1);
	}
#endif

	inline bool Hook()
	{
		bool anySuccess = false;
		if (const auto hModule = GetModuleHandleA(game_overlay_renderer))
		{
			LOG_NOTICE("Detected Steam overlay...");
			if (Overlay::graphicsAPI & D3D9)
			{
				if (const uintptr_t xRefDevicePresent = mem::PatternScan<uintptr_t>(hModule, d3d9_present_pattern))
				{
					const auto pPresent = GetPointerFromRef(xRefDevicePresent);
					const auto pReset = pPresent - 1; // previous ptr
					SwapPointer(pPresent, PTR_AND_NAME(DirectX9::Present));
					SwapPointer(pReset, PTR_AND_NAME(DirectX9::Reset));
					anySuccess = true;
				}

				if (const uintptr_t xRefSwapChainPresent = mem::PatternScan<uintptr_t>(hModule, d3d9_swapchain_present_pattern))
				{
					const auto pPresent = GetPointerFromRef(xRefSwapChainPresent);
					SwapPointer(pPresent, PTR_AND_NAME(DirectX9::SwapChainPresent));
					anySuccess = true;
				}
			}
			if (Overlay::graphicsAPI & D3D11 || Overlay::graphicsAPI & D3D12)
			{
				if (const uintptr_t xRefPresent = mem::PatternScan<uintptr_t>(hModule, d3d_present_pattern))
				{
					const auto pPresent = GetPointerFromRef(xRefPresent);
					const auto pResizeBuffers = pPresent + 1; // next ptr
					if (Overlay::graphicsAPI & D3D11)
					{
						SwapPointer(pPresent, PTR_AND_NAME(DirectX11::PresentHook));
						SwapPointer(pResizeBuffers, PTR_AND_NAME(DirectX11::ResizeBuffersHook));
						anySuccess = true;
					}
#ifdef _WIN64
					if (Overlay::graphicsAPI & D3D12 && DirectX12::CreateFactoryAndCommandQueue())
					{
						SwapPointer(pPresent, PTR_AND_NAME(DirectX12::Present));
						SwapPointer(pResizeBuffers, PTR_AND_NAME(DirectX12::ResizeBuffers));
						anySuccess = true;
					}
#endif
				}
			}
			if (Overlay::graphicsAPI & GraphicsAPI::OpenGL)
			{
				if (const uintptr_t xRefSwapBuffers = mem::PatternScan<uintptr_t>(hModule, opengl_swap_buffers_pattern))
				{
					const auto pWglSwapBuffers = GetPointerFromRef(xRefSwapBuffers);
					SwapPointer(pWglSwapBuffers, PTR_AND_NAME(OpenGL::WglSwapBuffers));
					anySuccess = true;
				}
			}

			if (Overlay::graphicsAPI & GraphicsAPI::Vulkan)
			{
				if (const auto hSteamOverlayVulkan = GetModuleHandleA(steam_overlay_vulkan_layer))
				{
#ifdef VULKAN_BACKEND_ENABLED
					const uintptr_t pQueuePresentKHR = reinterpret_cast<uintptr_t>(GetProcAddress(hSteamOverlayVulkan, "vkQueuePresentKHR"));
					const uintptr_t pCreateSwapchainKHR = reinterpret_cast<uintptr_t>(GetProcAddress(hSteamOverlayVulkan, "vkCreateSwapchainKHR"));
					if (pQueuePresentKHR && pCreateSwapchainKHR)
					{
						HooksManager::Create<InlineHook>(pQueuePresentKHR, PTR_AND_NAME(Overlay::Vulkan::vkQueuePresentKHR));
						HooksManager::Create<InlineHook>(pCreateSwapchainKHR, PTR_AND_NAME(Overlay::Vulkan::vkCreateSwapchainKHR));
						Vulkan::Hook(); // Hooking remaining functions
						anySuccess = true;
					}
#else
					LOG_WARNING("Vulkan Steam Overlay detected but backend is DISABLED - install Volk library or #include \"volk.h\".");
#endif
					
				}
			}
			if (!anySuccess) LOG_WARNING("Couldn't find Steam Overlay's function addresses...");
		}
		return anySuccess;
	}
}
