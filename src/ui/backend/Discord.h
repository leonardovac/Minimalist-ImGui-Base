#pragma once
#include <windows.h>
#include <Mem/mem.h>

#include "../overlay.h"

namespace Overlay::Discord
{
	inline bool Hook()
	{
		if (const auto hModule = GetModuleHandleA("DiscordHook64.dll"))
		{
			if (Overlay::graphicsAPI == D3D11)
			{
				if (void* pPresent = mem::PatternScan(hModule, "55 41 ?? 41 ?? 56 57 53 48 83 EC ?? 48 8D ?? ?? ?? 44 89"))
				{
					if (void* pResizeBuffers = mem::PatternScan(hModule, "55 41 ?? 56 57 53 48 83 EC ?? 48 8D ?? ?? ?? 44 89 ?? 44 89 ?? 89 D3 49 89 ?? E8 ?? ?? ?? ?? 8B 4D"))
					{
						LOG_NOTICE("Hooking Discord overlay...");
						HooksManager::Setup<InlineHook>(pPresent, FUNCTION(DirectX11::PresentHook));
						HooksManager::Setup<InlineHook>(pResizeBuffers, FUNCTION(DirectX11::ResizeBuffersHook));
						return true;
					}
				}
			}
		}
		return false;
	}
}
