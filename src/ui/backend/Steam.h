#pragma once
#include <windows.h>
#include <Mem/mem.h>

#include "../overlay.h"

namespace Overlay::Steam
{
#ifdef _WIN64
	constexpr auto moduleName = "GameOverlayRenderer64.dll";
	constexpr auto d3dPresentPattern = "48 89 ?? ?? ?? 48 89 ?? ?? ?? 48 ?? 74 24 ?? ?? 41 56 ?? 57 48 83 EC ?? 41 8B E8";
	constexpr auto d3dResizeBuffersPattern = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 44 8B FA";
	constexpr auto oglSwapBuffersPattern = "40 53 48 83 EC ?? 48 8B D9 48 8D 54 24 ?? 48 8B 0D";
#else
	constexpr auto moduleName = "GameOverlayRenderer.dll";
	constexpr auto d3dPresentPattern = "55 8B EC 64 A1 ? ? ? ? 6A ? 68 ? ? ? ? 50 64 89 25 ? ? ? ? 53 8B 5D ? 56 57 F6 C3 ? 74 ? A1";
	constexpr auto d3dResizeBuffersPattern = "55 8B EC 53 8B 5D ? B9 ? ? ? ? 56 57 53 E8 ? ? ? ? 53 B9 ? ? ? ? 8B F8 E8 ? ? ? ? 8B F0 85 FF 74 ? 8B CF E8 ? ? ? ? 85 F6 74 ? 8B CE E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? FF 75 ? A1 ? ? ? ? FF 75 ? FF 75";
	constexpr auto oglSwapBuffersPattern = "55 8B EC 83 EC ?? 8D 45 ?? 50 FF 35";
#endif

	inline bool Init()
	{
		if (const auto hModule = GetModuleHandleA(moduleName))
		{
			LOG_NOTICE("Detected Steam overlay...");
			if (Overlay::graphicsAPI == D3D11 || Overlay::graphicsAPI == D3D12)
			{
				if (void* pPresent = mem::PatternScan(hModule, d3dPresentPattern))
				{
					if (void* pResizeBuffers = mem::PatternScan(hModule, d3dResizeBuffersPattern))
					{
						if (Overlay::graphicsAPI == D3D11)
						{
							const bool presentStatus		= HooksManager::Setup<InlineHook>(pPresent, FUNCTION(DirectX11::PresentHook));
							const bool resizeBuffersStatus	= HooksManager::Setup<InlineHook>(pResizeBuffers, FUNCTION(DirectX11::ResizeBuffersHook));
							return presentStatus && resizeBuffersStatus;
						}
#ifdef _WIN64
						if (Overlay::graphicsAPI == D3D12 && DirectX12::CreateFactoryAndCommandQueue())
						{
							const bool presentStatus		= HooksManager::Setup<InlineHook>(pPresent, FUNCTION(DirectX12::Present));
							const bool resizeBuffersStatus	= HooksManager::Setup<InlineHook>(pResizeBuffers, FUNCTION(DirectX12::ResizeBuffers));
							return presentStatus && resizeBuffersStatus;
						}
#endif
					}
				}
			}
			else if (Overlay::graphicsAPI == GraphicsAPI::OpenGL)
			{
				if (void* wglSwapBuffers = mem::PatternScan(hModule, oglSwapBuffersPattern))
				{
					return HooksManager::Setup<InlineHook>(wglSwapBuffers, FUNCTION(OpenGL::WglSwapBuffers));
				}
			}
		}
		return false;
	}
}
