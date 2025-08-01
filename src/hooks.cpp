#include "hooks.h"

#include "Mem/mem.h"

namespace // Some utility functions
{
	std::unordered_map<const char*, HMODULE> moduleRegistry{};

	HMODULE TryGetModuleHandle(const char* moduleName)
	{
		if (const auto it = moduleRegistry.find(moduleName); it != moduleRegistry.end())
		{
			return it->second;
		}

		if (const HMODULE handle = GetModuleHandleA(moduleName))
		{
			return moduleRegistry[moduleName] = handle;
		}
		return nullptr;
	}
}

namespace Detours
{
	static void ExampleMidDetour(SafetyHookContext& ctx)
	{
		// This is a mid-hook detour, so we can modify the context before the original function is called
		// Check: https://learn.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-170 and https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
#ifdef _WIN64
		//ctx.rdx = reinterpret_cast<uintptr_t>(L"Hi from ExampleMidDetour!");
		ctx.r8 = reinterpret_cast<uintptr_t>(L"[HOOKED]");
#endif
	}

	static int WINAPI ExampleInlineDetour(const HWND hWnd, const LPCSTR lpText, const LPCSTR lpCaption, const UINT uType)
	{
		static auto& original = HooksManager::GetOriginal(ExampleInlineDetour);
		// Any calling convention works for x64 (as it defaults to __fastcall), but for x86 you need to specify the calling convention explicitly, also WINAPI === __stdcall
		original.stdcall<int>(hWnd, "Hi from ExampleInlineDetour!", "[HOOKED]", uType);
		return original.stdcall<int>(hWnd, lpText, lpCaption, uType);
	}
}

namespace Hooks
{
	enum class HookType : std::uint8_t
	{
		Inline,
		Mid
	};

	struct HookEntry
	{
		void* address;
		void* detour;
		const char* name{};
		HookType type;

		template<typename T>
		HookEntry(void* address, T detour, const char* name, const HookType hookType) : address(address), detour(reinterpret_cast<void*>(detour)), name(name), type(hookType) {}

		template<typename T>
		HookEntry(void* address, T detour, const HookType hookType) : HookEntry(address, detour, "Unknown", hookType) {}

		template<typename T>
		HookEntry(const HMODULE& module, const char* pattern, T detour, const char* name, const HookType hookType) : detour(reinterpret_cast<void*>(detour)), name(name), type(hookType)
		{
			if (module) address = mem::PatternScan(module, pattern);
		}

		template<typename T>
		HookEntry(const char* moduleName, const char* pattern, T detour, const char* name, const HookType hookType) : HookEntry(TryGetModuleHandle(moduleName), pattern, detour, name, hookType) {}

		template<typename T>
		HookEntry(const char* moduleName, const char* pattern, T detour, const HookType hookType) : HookEntry(moduleName, pattern, detour, "Unknown", hookType) {}

		template<typename T>
		HookEntry(const char* pattern, T detour, const HookType hookType) : HookEntry(static_cast<const char*>(nullptr), pattern, detour, hookType) {}
	};

	static inline HookEntry List[]
	{
		{
			MessageBoxA, // Address of the original function
			PTR_AND_NAME(Detours::ExampleInlineDetour), // Macro for getting the address of the detour function and its name
			HookType::Inline
		},
		{
			MessageBoxW,
			Detours::ExampleMidDetour,
			"Detours::ExampleMidDetour", // Name for logging purposes, can be nullptr
			HookType::Mid
		},
		{
			"DEAD BEEF ?? BABE FACE", // Just a placeholder, will search for it in the executable module (e.g: game.exe)
			Detours::ExampleMidDetour,
			HookType::Mid
		},
		{
			"module.dll", // Name of the module to search for the pattern
			"DEAD C0DE ?? B01D FACE", // Will search for it in the specified module
			Detours::ExampleMidDetour,
			HookType::Mid
		},
	};

	extern void SetupAllHooks()
	{
		LOG_NOTICE("Starting hooking procedures...");
		for (auto& [address, detour, name, type] : Hooks::List)
		{
			switch (type)
			{
			case HookType::Inline:
				HooksManager::Create<InlineHook>(address, detour, name);
				break;
			case HookType::Mid:
				HooksManager::Create<MidHook>(address, detour, name);
				break;
			}
		}
	}
}
