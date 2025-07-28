#pragma once
#include <windows.h>

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
	constexpr auto module_name = "GameOverlayRenderer64.dll";
	constexpr auto d3d_present_pattern = "48 8B 05 ? ? ? ? 44 8B C5 8B D6";
	constexpr auto d3d_resize_buffers_pattern = "4C 8B 15 ? ? ? ? 45 8B C6";
	constexpr auto d3d9_device_present_pattern = "48 8B 05 ? ? ? ? 4C 8B C5 49 8B D7";
	constexpr auto opengl_swap_buffers_pattern = "48 8B 05 ? ? ? ? 48 8B CB FF D0 80 3D";

	static uintptr_t* GetAddressFromRef(const uintptr_t instructionStart)
	{
		return reinterpret_cast<uintptr_t*>(instructionStart + 7 + *reinterpret_cast<int32_t*>(instructionStart + 3));
	}
#else
	constexpr auto module_name = "GameOverlayRenderer.dll";
	constexpr auto d3d_present_pattern = "A1 ? ? ? ? 53 FF 75 ? FF 75 ? FF D0 8B 4D ? 64 89 0D ? ? ? ? 5F 5E 5B 8B E5 5D C2 ? ? 68 ? ? ? ? C7 45 ? ? ? ? ? FF 15 ? ? ? ? 8B 75";
	constexpr auto d3d_resize_buffers_pattern = "A1 ? ? ? ? FF 75 ? FF 75 ? FF 75 ? FF 75 ? 53 FF D0";
	constexpr auto d3d9_device_present_pattern = "A1 ? ? ? ? 51 53 FF D0";
	constexpr auto opengl_swap_buffers_pattern = "A1 ? ? ? ? 56 FF D0 80 3D";

	static uintptr_t* GetAddressFromRef(const uintptr_t instructionStart)
	{
		return *reinterpret_cast<uintptr_t**>(instructionStart + 1);
	}
#endif

	inline bool Hook()
	{
		bool anySuccess = false;
		if (const auto hModule = GetModuleHandleA(module_name))
		{
			LOG_NOTICE("Detected Steam overlay...");
			if (Overlay::graphicsAPI & D3D9)
			{
				if (const uintptr_t xRefDevicePresent = mem::PatternScan<uintptr_t>(hModule, d3d9_device_present_pattern))
				{
					const auto pOriginalPresent = GetAddressFromRef(xRefDevicePresent);
					SwapPointer(pOriginalPresent, FUNCTION(DirectX9::Present));
					anySuccess = true;
				}
			}
			if (Overlay::graphicsAPI & D3D11 || Overlay::graphicsAPI & D3D12)
			{
				if (const uintptr_t xRefPresent = mem::PatternScan<uintptr_t>(hModule, d3d_present_pattern))
				{
					if (const uintptr_t xRefResizeBuffers = mem::PatternScan<uintptr_t>(hModule, d3d_resize_buffers_pattern))
					{
						const auto pOriginalPresent = GetAddressFromRef(xRefPresent);
						const auto pOriginalResizeBuffers = GetAddressFromRef(xRefResizeBuffers);
						if (Overlay::graphicsAPI & D3D11)
						{
							SwapPointer(pOriginalPresent, FUNCTION(DirectX11::PresentHook));
							SwapPointer(pOriginalResizeBuffers, FUNCTION(DirectX11::ResizeBuffersHook));
							anySuccess = true;
						}
#ifdef _WIN64
						if (Overlay::graphicsAPI & D3D12 && DirectX12::CreateFactoryAndCommandQueue())
						{
							SwapPointer(pOriginalPresent, FUNCTION(DirectX12::Present));
							SwapPointer(pOriginalResizeBuffers, FUNCTION(DirectX12::ResizeBuffers));
							anySuccess = true;
						}
#endif
					}
				}
			}
			if (Overlay::graphicsAPI & GraphicsAPI::OpenGL)
			{
				if (const uintptr_t xRefSwapBuffers = mem::PatternScan<uintptr_t>(hModule, opengl_swap_buffers_pattern))
				{
					const auto pOriginalPresent = GetAddressFromRef(xRefSwapBuffers);
					SwapPointer(pOriginalPresent, FUNCTION(OpenGL::WglSwapBuffers));
					anySuccess = true;
				}
			}
			if (!anySuccess) LOG_WARNING("Couldn't find Steam Overlay's function addresses...");
		}
		return anySuccess;
	}
}
