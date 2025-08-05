#include <windows.h>
#include <ScreenCleaner/ScreenCleaner.h>

#include "hooks.h"
#include "misc/logger.h"
#include "ui/overlay.h"

namespace
{
	std::string_view stristr(std::string_view haystack, const std::string_view needle)
	{
		if (needle.empty()) return haystack;

		const auto pos = haystack.find(needle);
		return pos != std::string_view::npos ? haystack.substr(pos) : std::string_view{};
	}

	std::string GetCurrentProcessName()
	{
		std::array<char, MAX_PATH> processPath{};
		DWORD size = static_cast<DWORD>(processPath.size());

		if (QueryFullProcessImageNameA(GetCurrentProcess(), 0, processPath.data(), &size))
		{
			// Extract filename from full path
			const std::filesystem::path fullPath(processPath.data());
			return fullPath.filename().string();
		}

		return {};
	}

	bool IsBlacklistedProcess()
	{
		const std::string processName = GetCurrentProcessName();
		constexpr std::array<const char*, 1> blacklist = {
			"UnityCrashHandler"
		};

		for (const auto& name : blacklist)
		{
			if (stristr(processName, name).data())
			{
				LOG_CRITICAL("Blacklisted process detected: {}", processName);
				ConsoleManager::destroy_console();
				return true;
			}
		}
		return false;
	}

	void ParseTitleForGraphicsAPI(const std::string_view windowTitle)
	{
		if (stristr(windowTitle, "DX9").data()) Overlay::graphicsAPI = GraphicsAPI::D3D9;
		else if (stristr(windowTitle, "DX11").data()) Overlay::graphicsAPI = GraphicsAPI::D3D11;
		else if (stristr(windowTitle, "DX12").data()) Overlay::graphicsAPI = GraphicsAPI::D3D12;
		else if (stristr(windowTitle, "OpenGL").data()) Overlay::graphicsAPI = GraphicsAPI::OpenGL;
		else if (stristr(windowTitle, "Vulkan").data()) Overlay::graphicsAPI = GraphicsAPI::Vulkan;
	}

	bool CheckWindow(const HWND& hWindow, const DWORD& processId)
	{
		if (hWindow && hWindow != hConsole && IsWindowVisible(hWindow) && !IsIconic(hWindow))
		{
			DWORD windowProcess;
			GetWindowThreadProcessId(hWindow, &windowProcess);
			if (windowProcess == processId)
			{
				std::array<char, 256> windowTitle{};
				if (GetWindowTextA(hWindow, windowTitle.data(), static_cast<int>(windowTitle.size())))
				{
					LOG_NOTICE("Found game window: {}", windowTitle.data());
					ParseTitleForGraphicsAPI(windowTitle.data()); // Just in case...
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

namespace Hooks {
	extern void SetupAllHooks();
}

ScreenCleaner screenCleaner(&Overlay::bEnabled);
extern void MainThread()
{
	if (IsBlacklistedProcess()) return;
	
#if LOGGING_ENABLED
	ConsoleManager::create_console();
	SetupQuill("log.txt");
#endif

	Hooks::SetupAllHooks();
	ScreenCleaner::Init();
	std::thread(HookRendering).detach();
}
