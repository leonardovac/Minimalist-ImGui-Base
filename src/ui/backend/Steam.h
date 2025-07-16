#pragma once
#include <windows.h>
#include <Mem/mem.h>

#include "../overlay.h"

namespace Overlay::Steam
{
	inline bool Init()
	{
		if (const auto hModule = GetModuleHandleA("GameOverlayRenderer64.dll"))
		{
			if (Overlay::graphicsAPI == D3D11)
			{
				if (void* pPresent = mem::PatternScan(hModule, "48 89 ?? ?? ?? 48 89 ?? ?? ?? 48 ?? 74 24 ?? ?? 41 56 ?? 57 48 83 EC ?? 41 8B E8"))
				{
					if (void* pResizeBuffers = mem::PatternScan(hModule, "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 44 8B FA"))
					{
						LOG_NOTICE("Hooking Steam overlay...");
						const bool presentStatus		= HooksManager::Setup<InlineHook>(pPresent, FUNCTION(DirectX11::PresentHook));
						const bool resizeBuffersStatus	= HooksManager::Setup<InlineHook>(pResizeBuffers, FUNCTION(DirectX11::ResizeBuffersHook));
						return presentStatus && resizeBuffersStatus;
					}
				}
			}
		}
		return false;
	}
}
