#include <windows.h>
#include <ScreenCleaner/ScreenCleaner.h>

#include "game.h"
#include "hooks.h"
#include "misc/logger.h"
#include "ui/overlay.h"

namespace
{
	bool CheckWindow(const HWND& hWindow, const DWORD& processId)
	{
		if (hWindow && hWindow != hConsole && IsWindowVisible(hWindow) && !IsIconic(hWindow))
		{
			DWORD windowProcess;
			GetWindowThreadProcessId(hWindow, &windowProcess);
			if (windowProcess == processId)
			{
				char windowTitle[256];
				GetWindowTextA(hWindow, windowTitle, sizeof(windowTitle));

				if (strlen(windowTitle) > 0)
				{
					LOG_NOTICE("Found game window: {}", windowTitle);
					Game::hWindow = hWindow;
					return true;
				}
			}
		}
		return false;
	}

	void HookPresent()
	{
		constexpr std::chrono::seconds timeout{ 10 };
		const auto startTime = std::chrono::steady_clock::now();

		const DWORD currentProcessId = GetCurrentProcessId();

		LOG_INFO("Searching for game window...");

		while (!Game::hWindow) 
		{
			const HWND foregroundWindow = GetForegroundWindow();
			if (CheckWindow(foregroundWindow, currentProcessId)) break;

			const auto EnumWindowsProc = [](const HWND hWindow, const LPARAM lParam) -> BOOL
			{
				return !CheckWindow(hWindow, *reinterpret_cast<DWORD*>(lParam));
			};
			EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&currentProcessId));

			if (const auto duration = std::chrono::steady_clock::now() - startTime; duration > timeout)
			{
				LOG_CRITICAL("Couldn't find game window after {}s.", std::chrono::duration_cast<std::chrono::seconds>(duration).count());
				return;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(1)); // Ensuring it has time to initialize

		if (!Overlay::TryAllPresentMethods())
		{
			LOG_CRITICAL("Couldn't hook game rendering.");
		}
	}
}

ScreenCleaner screenCleaner(&Overlay::bEnabled);
extern void MainThread()
{
#if LOGGING_ENABLED
	ConsoleManager::create_console();
	SetupQuill("log.txt");
#endif

	Hooks::SetupAllHooks();
	std::thread(HookPresent).detach();
	if (screenCleaner.Init()) screenCleaner.Enable();
}
