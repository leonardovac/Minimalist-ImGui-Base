#include "base.h"

#include <windows.h>
#include <SafetyHook/Wrapper.hpp>
#include <ScreenCleaner/ScreenCleaner.h>

#include "hooks.h"
#include "misc/logger.h"
#include "ui/overlay.h"

namespace
{
	void HookPresent()
	{
		const auto startTime = std::chrono::steady_clock::now();
		constexpr std::chrono::seconds timeout{ 10 };

		while (!Game::hWindow) {
			const auto EnumWindowsProc = [](const HWND hWindow, const LPARAM lParam) -> BOOL
			{
				const auto gameWindow = reinterpret_cast<HWND*>(lParam);

				DWORD processId;
				GetWindowThreadProcessId(hWindow, &processId);

				if (processId == GetCurrentProcessId() && hWindow != GetConsoleWindow() && IsWindowVisible(hWindow) && !IsIconic(hWindow))
				{
					RECT rect;
					if (GetWindowRect(hWindow, &rect) && rect.right >= rect.left && rect.bottom >= rect.top && rect.right - rect.left > 100 && rect.bottom - rect.top > 100) 
					{
						wchar_t windowTitle[256];
						GetWindowTextW(hWindow, windowTitle, sizeof(windowTitle));

						if (wcslen(windowTitle) > 0)
						{
							*gameWindow = hWindow;
							return FALSE; // Stop
						}
					}
				}
				return TRUE; // Continue
			};

			EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&Game::hWindow));

			if (std::chrono::steady_clock::now() - startTime > timeout) 
			{
				LOG_WARNING("Window search is taking too long, trying GetForegroundWindow()...");
				if (!((Game::hWindow = GetForegroundWindow())))
				{
					LOG_CRITICAL("Couldn't find game window.");
					return;
				}
			}

			// When console is created and not hidden, we need to make sure the game window is the main one.
			SetForegroundWindow(Game::hWindow);

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (!Overlay::TryAllPresentMethods())
		{
			LOG_CRITICAL("Couldn't hook game rendering.");
		}
	}
}

ScreenCleaner screenCleaner(&Overlay::bEnabled);
void MainThread()
{
#if LOGGING_ENABLED
	ConsoleManager::create_console();
	SetupQuill("log.txt");
#endif

	Hooks::SetupAllHooks();
	std::thread(HookPresent).detach();
	if (screenCleaner.Init()) screenCleaner.Enable();
}
