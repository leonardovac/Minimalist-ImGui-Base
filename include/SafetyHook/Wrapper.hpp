#pragma once
#include <shared_mutex>

#include "safetyhook.hpp"

using safetyhook::InlineHook;
using safetyhook::MidHook;
using safetyhook::VmtHook;
using safetyhook::VmHook;
using safetyhook::Context;

// Base class for all hooks
class HookBase
{
public:
	virtual ~HookBase() = default;

	virtual void        unhook()		= 0;
	virtual InlineHook& getHookInline() = 0;
	virtual MidHook&	getHookMid()	= 0;
	virtual VmtHook&	getHookVmt()	= 0;
	virtual VmHook&		getHookVm()		= 0;
	virtual uint8_t     error()			= 0;
};

// Template class for specific function hooks
template <typename HookType>
class FunctionHook final : public HookBase
{
public:
	// Constructor for inline and mid hooks
	FunctionHook(void* target, void* destination)
	{
		if constexpr (std::is_same_v<HookType, InlineHook>)
		{
			if (auto result = InlineHook::create(target, destination))
			{
				originalFunctionInline = std::move(*result);
			}
			else errorType = result.error().type;
		}
		else if constexpr (std::is_same_v<HookType, MidHook>)
		{
			if (auto result = MidHook::create(target, reinterpret_cast<safetyhook::MidHookFn>(destination)))
			{
				originalFunctionMid = std::move(*result);
			}
			else errorType = result.error().type;
		}
	}

	// Constructor for VMT hook
	template<typename T>
	FunctionHook(T* targetObject) requires std::is_same_v<HookType, VmtHook>
	{
		if (auto result = VmtHook::create(targetObject))
		{
			originalFunctionVmt = std::move(*result);
		}
		else errorType = result.error().type;
	}

	// Constructor for VM hook
	template<typename T>
	FunctionHook(VmtHook& vmtHook, std::size_t index, T hookFn) requires std::is_same_v<HookType, VmHook>
	{
		if (auto result = vmtHook.hook_method(index, hookFn))
		{
			originalFunctionVm = std::move(*result);
		}
		else errorType = result.error().type;
	}

	InlineHook& getHookInline() override
	{
		return originalFunctionInline;
	}

	MidHook& getHookMid() override
	{
		return originalFunctionMid;
	}

	VmtHook& getHookVmt() override
	{
		return originalFunctionVmt;
	}

	VmHook& getHookVm() override
	{
		return originalFunctionVm;
	}

	void unhook() override
	{
		originalFunctionInline = {};
		originalFunctionMid = {};
		originalFunctionVmt = {};
		originalFunctionVm = {};
	}

	uint8_t error() override
	{
		return errorType;
	}

private:
	InlineHook originalFunctionInline;
	MidHook    originalFunctionMid;
	VmtHook    originalFunctionVmt;
	VmHook     originalFunctionVm;

	uint8_t    errorType = 0;
};

namespace HooksManager
{
	inline int fails;
	inline std::unordered_map<const void*, std::vector<std::unique_ptr<HookBase>>> hooks;
	// Separate storage for VMT hooks since they don't have a replacement function pointer
	inline std::unordered_map<const void*, std::vector<std::unique_ptr<HookBase>>> vmt_hooks;
	// To avoid a (possible) crash on Inject
	static std::shared_mutex hooking;

	inline const char* ParseError(const uint8_t& type)
	{
		switch (type)
		{
		case InlineHook::Error::BAD_ALLOCATION: return "BAD ALLOCATION";
		case InlineHook::Error::FAILED_TO_DECODE_INSTRUCTION: return "FAILED TO DECODE INSTRUCTION";
		case InlineHook::Error::SHORT_JUMP_IN_TRAMPOLINE: return "SHORT JUMP IN TRAMPOLINE";
		case InlineHook::Error::IP_RELATIVE_INSTRUCTION_OUT_OF_RANGE: return "IP RELATIVE INSTRUCTION OUT OF RANGE";
		case InlineHook::Error::UNSUPPORTED_INSTRUCTION_IN_TRAMPOLINE: return "UNSUPPORTED INSTRUCTION IN TRAMPOLINE";
		case InlineHook::Error::FAILED_TO_UNPROTECT: return "FAILED TO UNPROTECT";
		case InlineHook::Error::NOT_ENOUGH_SPACE: return "NOT ENOUGH SPACE";
		default:;
		}
		return {};
	}

	// Original Setup for inline and mid hooks
	template <typename HookType>
	void Setup(void* original, void* replacement, const bool duplicated = false)
	{
		if (!original)
		{
			fails++;
			return;
		}

		if (!duplicated && hooks.contains(replacement))
		{
			fails++;
			return;
		}

		// Locks until out of scope
		std::unique_lock lock(hooking);
		hooks[replacement].emplace_back(std::make_unique<FunctionHook<HookType>>(original, replacement));

		if (hooks[replacement].back()->error())
		{
			fails++;
		}
	}

	// Setup for VMT hooks - returns reference to the created VMT hook
	template <typename T>
	VmtHook& SetupVmt(T* targetObject)
	{
		static VmtHook emptyHook{}; // Static empty hook for error cases

		if (!targetObject)
		{
			fails++;
			return emptyHook;
		}

		// Locks until out of scope
		std::unique_lock lock(hooking);
		vmt_hooks[targetObject].emplace_back(std::make_unique<FunctionHook<VmtHook>>(targetObject));

		if (vmt_hooks[targetObject].back()->error())
		{
			fails++;
			return emptyHook;
		}

		return vmt_hooks[targetObject].back()->getHookVmt();
	}

	// Setup for VM hooks (requires existing VMT hook)
	template <typename T>
	void SetupVm(VmtHook& vmtHook, std::size_t index, T hookFn)
	{
		// Locks until out of scope
		std::unique_lock lock(hooking);
		hooks[hookFn].emplace_back(std::make_unique<FunctionHook<VmHook>>(vmtHook, index, hookFn));

		if (hooks[hookFn].back()->error())
		{
			fails++;
		}
	}

	template <typename HookType>
	HookType& GetOriginal(const void* replacement)
	{
		// Check if it's locked (wait until hook is completed if that's the case)
		std::shared_lock lock(hooking);

		if constexpr (std::is_same_v<HookType, InlineHook>)
		{
			return hooks[replacement].back()->getHookInline();
		}
		else if constexpr (std::is_same_v<HookType, MidHook>)
		{
			return hooks[replacement].back()->getHookMid();
		}
		else if constexpr (std::is_same_v<HookType, VmHook>)
		{
			return hooks[replacement].back()->getHookVm();
		}
		else return HookType{};
	}

	inline InlineHook& GetOriginal(const void* replacement)
	{
		// Check if it's locked (wait until hook is completed if that's the case)
		std::shared_lock lock(hooking);
		return hooks[replacement].back()->getHookInline();
	}

	// Get VMT hook by target object
	inline VmtHook& GetVmtHook(const void* targetObject)
	{
		std::shared_lock lock(hooking);
		return vmt_hooks[targetObject].back()->getHookVmt();
	}

	inline int GetFailCount()
	{
		return fails;
	}

	inline size_t GetHookCount()
	{
		std::shared_lock lock(hooking);
		return hooks.size() + vmt_hooks.size();
	}

	inline void UnhookAll()
	{
		hooks.clear();
		vmt_hooks.clear();
	}

	inline void Unhook(const void* replacement)
	{
		if (hooks.contains(replacement))
		{
			hooks.erase(replacement);
		}
		else if (vmt_hooks.contains(replacement))
		{
			vmt_hooks.erase(replacement);
		}
	}
};