#include "base.h"

#include <windows.h>

#include <SafetyHook/Wrapper.hpp>
#include <ScreenCleaner/ScreenCleaner.h>
#include "hooks.h"
#include "ui/overlay.h"

namespace
{
	void HookPresent()
	{
		const auto startTime = std::chrono::steady_clock::now();
		constexpr std::chrono::seconds timeout{ 10 };

		const DWORD currentProcessId = GetCurrentProcessId();

#ifdef _WIN64
		while (!Overlay::hGameWindow)
		{
			HWND hWindow = FindWindowEx(nullptr, nullptr, nullptr, nullptr);

			while (hWindow)
			{
				DWORD processId;
				GetWindowThreadProcessId(hWindow, &processId);

				if (processId == currentProcessId && IsWindowVisible(hWindow) && !IsIconic(hWindow))
				{
					if (RECT rect; GetWindowRect(hWindow, &rect) && rect.right >= rect.left && rect.bottom >= rect.top && rect.right - rect.left > 100 && rect.bottom - rect.top > 100)
					{
						wchar_t windowTitle[256];
						GetWindowTextW(hWindow, windowTitle, sizeof(windowTitle));

						if (wcslen(windowTitle) > 0)
						{
							Overlay::hGameWindow = hWindow;
							break;
						}
					}
				}

				hWindow = FindWindowEx(nullptr, hWindow, nullptr, nullptr);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			if (std::chrono::steady_clock::now() - startTime > timeout) { return; }

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
#endif

		if (!Overlay::TryAllPresentMethods())
		{
			MessageBoxA(nullptr, "Couldn't hook game rendering.", nullptr, 0);
		}
	}
}

ScreenCleaner screenCleaner(&Overlay::bEnabled);
void MainThread()
{
	Hooks::SetupAllHooks();
	std::thread(HookPresent).detach();
	if (screenCleaner.Init()) screenCleaner.Enable();
}
