#pragma once
#include "game.h"
#include "Mem/mem.h"
#include "SafetyHook/safetyhook.hpp"
#include "SafetyHook/Wrapper.hpp"
#include "ui/menu.h"

namespace Hooks::Helpers
{
	
}

namespace Hooks::Detours
{
	inline void ExampleMidDetour(SafetyHookContext& ctx)
	{
		// This is a mid-hook detour, so we can modify the context before the original function is called
		// Check: https://learn.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-170 and https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
#ifdef _WIN64
		//ctx.rdx = reinterpret_cast<uintptr_t>(L"Hi from ExampleMidDetour!");
		ctx.r8 = reinterpret_cast<uintptr_t>(L"[HOOKED]");
#endif
	}

	inline int WINAPI ExampleInlineDetour(const HWND hWnd, const LPCSTR lpText, const LPCSTR lpCaption, const UINT uType)
	{
		auto& original = HooksManager::GetOriginal(ExampleInlineDetour);
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
		const char* pattern {};
		void* address {};
		HMODULE module {};
		void* detour;
		HookType type;
	};

	inline HookEntry List[]
	{
		{
			.address = MessageBoxA,
			.detour = Hooks::Detours::ExampleInlineDetour,
			.type = HookType::Inline
		},
		{
			.address = MessageBoxW,
			.detour = Hooks::Detours::ExampleMidDetour,
			.type = HookType::Mid
		},
		{
			.pattern = "DEAD BEEF ?? BABE FACE",
			.module = hExecutable,
			.detour = Hooks::Detours::ExampleMidDetour,
			.type = HookType::Mid
		},
	};


	static void SetupAllHooks()
	{
		for (auto& [pattern, address, module, detour, type] : Hooks::List)
		{
			if (!address && pattern && module) { address = mem::PatternScan(module, pattern); }

			switch (type)
			{
			case HookType::Inline:
				HooksManager::Setup<InlineHook>(address, detour);
				break;
			case HookType::Mid:
				HooksManager::Setup<MidHook>(address, detour);
				break;
			}
		}
	}
}
