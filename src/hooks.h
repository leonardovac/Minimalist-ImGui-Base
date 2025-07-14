#pragma once
#include <shared_mutex>
#include <variant>
#include <SafetyHook/safetyhook.hpp>
#include <VMTHook/vmthook.h>

#include "misc/logger.h"

using safetyhook::InlineHook;
using safetyhook::MidHook;
using safetyhook::Context;

namespace Hooks
{
	void SetupAllHooks();
}

#define FUNCTION(func) &(func), \
        #func

enum : std::uint8_t {
	Default = 0,	///< Default flags.
	StartDisabled = 1,	///< Start the hook disabled.
	Duplicated = 2,	///< 2 or more hooks pointing to the same function.
};

// Variant to hold either InlineHook or MidHook
using InlineOrMidHook = std::variant<InlineHook, MidHook>;

// Base class for all hooks
class HookBase
{
public:
	virtual ~HookBase() = default;

	virtual void				unhook()	= 0;
	virtual InlineOrMidHook&	getHook()	= 0;
	virtual uint8_t				getError()	= 0;
};

// Template class for specific function hooks
template <typename HookType>
class FunctionHook final : public HookBase
{
public:
	// Constructor for inline and mid-hooks
	FunctionHook(void* target, void* destination, int flags = Default)
	{
		if constexpr (std::is_same_v<HookType, InlineHook>)
		{
			if (auto result = InlineHook::create(target, destination, static_cast<InlineHook::Flags>(flags)))
			{
				hook = std::move(*result);
			}
			else errorType = result.error().type;
		}
		else if constexpr (std::is_same_v<HookType, MidHook>)
		{
			if (auto result = MidHook::create(target, reinterpret_cast<safetyhook::MidHookFn>(destination), static_cast<MidHook::Flags>(flags)))
			{
				hook = std::move(*result);
			}
			else errorType = result.error().type;
		}
	}

	InlineOrMidHook& getHook() override
	{
		return hook;
	}

	void unhook() override
	{
		std::visit([](auto& hook) { hook = {}; }, hook);
	}

	uint8_t getError() override
	{
		return errorType;
	}

private:
	InlineOrMidHook hook;
	uint8_t		errorType = 0;
};

namespace HooksManager
{
	inline int fails;
	inline std::unordered_map<const void*, std::vector<std::unique_ptr<HookBase>>> hooks;
	// To avoid a (possible) crash on Inject
	static std::shared_mutex hooking;

#define RETURN_FAIL(...) { fails++; return __VA_ARGS__; }

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

	template <typename HookType>
	void Setup(void* original, void* replacement, const char* detourName = "Unknown", int flags = Default)
	{
		if (!original)
		{
			LOG_ERROR("Invalid address passed for {}: 0x{:X}", detourName, reinterpret_cast<uintptr_t>(original));
			RETURN_FAIL()
		}

		if (!(flags & Duplicated) && hooks.contains(replacement))
		{
			LOG_ERROR("Possible unintended duplicated hook for {}.", detourName);
			RETURN_FAIL()
		}

		flags &= ~Duplicated; // Clear Duplicated flag as it's not used anymore

		// Locks until out of scope
		std::unique_lock lock(hooking);
		hooks[replacement].emplace_back(std::make_unique<FunctionHook<HookType>>(original, replacement, flags));

		if (const uint8_t error = hooks[replacement].back()->getError())
		{
			LOG_ERROR("Couldn't hook function at 0x{:X} for {} | {}", reinterpret_cast<uintptr_t>(original), detourName, ParseError(error));
			RETURN_FAIL()
		}

		const char* hookType = std::is_same_v<HookType, InlineHook> ? "Inline" : "Mid";
		LOG_INFO("{}Hook placed at 0x{:X} -> {} (0x{:X})", hookType, reinterpret_cast<uintptr_t>(original), detourName, reinterpret_cast<uintptr_t>(replacement));
	}

	template <typename HookType>
	void Setup(void* original, void* replacement, const int flags = Default)
	{
		Setup<HookType>(original, replacement, nullptr, flags);
	}

	template <typename HookType>
	HookType& GetOriginal(const void* replacement)
	{
		std::shared_lock lock(hooking);

		auto& hookVariant = hooks[replacement].back()->getHook();
		return std::get<HookType>(hookVariant);
	}

	// Convenience function for InlineHook (most common case)
	inline InlineHook& GetOriginal(const void* replacement)
	{
		return GetOriginal<InlineHook>(replacement);
	}

	inline bool Enable(const void* replacement)
	{
		std::shared_lock lock(hooking);

		auto& hookVariant = hooks[replacement].back()->getHook();
		return std::visit([](auto& hook) -> bool {
			(void)hook.enable();
			return hook.enabled();
			}, hookVariant);
	}

	inline bool Disable(const void* replacement)
	{
		std::shared_lock lock(hooking);

		auto& hookVariant = hooks[replacement].back()->getHook();
		return std::visit([](auto& hook) -> bool {
			(void)hook.disable();
			return !hook.enabled();
			}, hookVariant);
	}

	inline int GetFailCount()
	{
		return fails;
	}

	inline size_t GetHookCount()
	{
		std::shared_lock lock(hooking);
		return hooks.size();
	}

	inline void UnhookAll()
	{
		hooks.clear();
	}

	inline void Unhook(const void* replacement)
	{
		if (hooks.contains(replacement))
		{
			hooks.erase(replacement);
		}
	}
}

using HookVariant = std::variant<VMTHook::OriginalMethod, std::reference_wrapper<InlineHook>>;

class OriginalFunc
{
public:
	// Constructor that takes hook identifier and resolves both internally
	template<typename HookType>
	OriginalFunc(HookType* hookIdentifier)
	{
		// Try VMT first, fall back to inline
		auto vmtHook = VMTHook::GetOriginal(hookIdentifier);
		if (vmtHook.isValid()) hookVariant = vmtHook;
		else hookVariant = std::ref(HooksManager::GetOriginal(hookIdentifier));
	}

	template <typename ReturnType = void, typename... Args>
	ReturnType call(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, VMTHook::OriginalMethod>)
			{
				return hook.template call<ReturnType>(args...);
			}
			else return hook.get().template unsafe_call<ReturnType>(args...);
		}, hookVariant);
	}

	template <typename ReturnType = void, typename... Args>
	ReturnType stdcall(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, VMTHook::OriginalMethod>)
			{
				return hook.template stdcall<ReturnType>(args...);
			}
			else return hook.get().template unsafe_stdcall<ReturnType>(args...);
		}, hookVariant);
	}

	template <typename ReturnType = void, typename... Args>
	ReturnType thiscall(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, VMTHook::OriginalMethod>)
			{
				return hook.template thiscall<ReturnType>(args...);
			}
			else return hook.get().template unsafe_thiscall<ReturnType>(args...);
		}, hookVariant);
	}

private:
	HookVariant hookVariant;
};