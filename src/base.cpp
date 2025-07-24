#include <windows.h>
#include <ScreenCleaner/ScreenCleaner.h>

#include "hooks.h"
#include "misc/logger.h"
#include "ui/overlay.h"

namespace
{
	const char* stristr(const char* haystack, const char* needle)
	{
		if (!haystack || !needle) return nullptr;
		if (*needle == '\0') return haystack;

		const size_t needleLen = strlen(needle);

		for (const char* p = haystack; *p; ++p)
		{
			if (_strnicmp(p, needle, needleLen) == 0)
			{
				return p;
			}
		}
		return nullptr;
	}

	void ParseTitleForGraphicsAPI(const char* windowTitle)
	{
		if (stristr(windowTitle, "DX9")) Overlay::graphicsAPI = GraphicsAPI::D3D9;
		else if (stristr(windowTitle, "DX11")) Overlay::graphicsAPI = GraphicsAPI::D3D11;
		else if (stristr(windowTitle, "DX12")) Overlay::graphicsAPI = GraphicsAPI::D3D12;
		else if (stristr(windowTitle, "OpenGL")) Overlay::graphicsAPI = GraphicsAPI::OpenGL;
		else if (stristr(windowTitle, "Vulkan")) Overlay::graphicsAPI = GraphicsAPI::Vulkan;
	}

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
					ParseTitleForGraphicsAPI(windowTitle); // Just in case...
					Overlay::hWindow = hWindow;
					return true;
				}
			}
		}
		return false;
	}

	void HookRendering()
	{
		constexpr std::chrono::seconds timeout{ 10 };
		const auto startTime = std::chrono::steady_clock::now();

		const DWORD currentProcessId = GetCurrentProcessId();

		LOG_INFO("Searching for game window...");

		while (!Overlay::hWindow) 
		{
			const HWND foregroundWindow = GetForegroundWindow();
			if (CheckWindow(foregroundWindow, currentProcessId)) break;

			const auto EnumWindowsProc = [](const HWND hWindow, const LPARAM lParam) -> BOOL
			{
				return !CheckWindow(hWindow, *reinterpret_cast<DWORD*>(lParam));
			};

			if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&currentProcessId))) // Enumeration stopped (window found)
			{
				break;
			}

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
	ScreenCleaner::Init();
	std::thread(HookRendering).detach();
}
