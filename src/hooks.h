#pragma once
#include <format>
#include <safetyhook.hpp>
#include <shared_mutex>
#include <string_view>
#include <variant>

#include "misc/logger.h"
#include "TinyHook/tinyhook.h"

// Classes
using safetyhook::InlineHook;
using safetyhook::MidHook;
using safetyhook::Context;
using TinyHook::EATHook;
using TinyHook::IATHook;
using TinyHook::VMTHook;

// Concepts
using TinyHook::tiny_hook;
using TinyHook::at_hook;
using TinyHook::vmt_hook;

template<typename T>
concept safety_hook = std::is_same_v<T, InlineHook> || std::is_same_v<T, MidHook>;

template <typename T>
concept function = requires(T t)
{
	requires std::is_pointer_v<T>;
	requires std::is_function_v<std::remove_pointer_t<T>>;
	{ static_cast<const void*>(t) } -> std::convertible_to<const void*>;
};

#define PTR_AND_NAME(func) &(func), \
        #func

#define RETURN_FAIL(...) { fails++; return __VA_ARGS__; }

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

	virtual void unhook() = 0;
	virtual InlineOrMidHook& getHook() = 0;
	virtual uint8_t getError() = 0;
};

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
	// To avoid a (possible) crash on Inject
	static std::shared_mutex hooking;
	// Single storage for Inline and mid-hooks
	inline std::unordered_map<const void*, std::vector<std::unique_ptr<HookBase>>> hooks;
	// Triple storage for EAT/IAT/VMT hooks
    template<tiny_hook HookType>
    inline std::unordered_map<std::string_view, std::unique_ptr<HookType>> tinyHooks{};

	namespace Utils
	{
		template<tiny_hook HookType>
		auto& GetHookStorage() { return tinyHooks<HookType>; }

		constexpr std::string_view ParseError(const uint8_t& type)
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
			default: return "UNKNOWN ERROR";
			}
		}
	}

	template <tiny_hook HookType, typename T>
	HookType* Setup(T* target, const std::string_view name = {})
	{
		std::unique_lock lock(hooking);
		std::string identifier{ name };

		if constexpr (std::is_same_v<HookType, VMTHook>)
		{
			if (identifier.empty())
			{
				int counter = 1;
				while (tinyHooks<VMTHook>.contains(identifier))
				{
					identifier = std::format("Unknown_{}", counter++);
				}
			}
		}
		else if (identifier.empty()) identifier = TinyHook::Utils::GetModuleFilename(target);
		
		auto [it, inserted] = Utils::GetHookStorage<HookType>().try_emplace(identifier, TinyHook::Setup<HookType>(target, identifier));
		if (inserted) LOG_INFO("Registered {} for: {}.", std::string(typeid(HookType).name()).substr(16), identifier);
		return it->second.get();
	}

	template <at_hook HookType>
	HookType* Setup(const char* module)
	{
		return Setup<HookType>(GetModuleHandleA(module), module);
	}

	template <at_hook HookType>
	HookType* Setup(const wchar_t* module)
	{
		return Setup<HookType>(GetModuleHandleW(module), module);
	}

	template <safety_hook HookType, typename T>
	bool Create(const T original, void* replacement, std::string_view detourName, int flags = Default)
	{
		auto address = [&]() -> void*
		{
			if constexpr (std::is_same_v<T, void*>) return original;
			else return reinterpret_cast<void*>(original);
		}();

		if (!address)
		{
			LOG_ERROR("Invalid address passed for {}: 0x{:X}.", detourName, reinterpret_cast<uintptr_t>(address));
			RETURN_FAIL(false)
		}

		if (!(flags & Duplicated) && hooks.contains(replacement))
		{
			LOG_WARNING("Possible unintended duplicated hook for {}.", detourName);
			RETURN_FAIL(false)
		}

		flags &= ~Duplicated; // Clear Duplicated flag as it's not used anymore

		// Locks until out of scope
		std::unique_lock lock(hooking);

		auto& hook = hooks[replacement].emplace_back(std::make_unique<FunctionHook<HookType>>(address, replacement, flags));
		if (const uint8_t error = hook->getError())
		{
			LOG_ERROR("Couldn't hook function at 0x{:X} for {} | {}.", reinterpret_cast<uintptr_t>(address), detourName, Utils::ParseError(error));
			hooks.erase(replacement);
			RETURN_FAIL(false)
		}

		LOG_INFO("{} placed at 0x{:X} -> {} (0x{:X})", std::string(typeid(HookType).name()).substr(18), reinterpret_cast<uintptr_t>(address), detourName, reinterpret_cast<uintptr_t>(replacement));
		return true;
	}

	template <safety_hook HookType, typename T>
	bool Create(const T original, void* replacement, const int flags = Default)
	{
		return Create<HookType>(original, replacement, "Unknown", flags);
	}

	template <at_hook HookType>
	bool Create(HookType* hook, std::string_view targetName, void* replacement, std::string_view detourName = {})
	{
		constexpr std::string hookType = std::string(typeid(HookType).name()).substr(16);
		if (const auto originalMethod = hook->Hook(targetName.data(), replacement); !originalMethod)
		{
			LOG_ERROR("Couldn't {}Hook {} ({}) to {} (0x{:X}), error: {}.", hookType, targetName, hook->name, detourName, reinterpret_cast<uintptr_t>(replacement), TinyHook::Utils::GetErrorMessage(originalMethod.error()));
			RETURN_FAIL(false)
		}

		LOG_INFO("{}Hooked {} ({}) -> {} (0x{:X}).", hookType, targetName, hook->name, detourName, reinterpret_cast<uintptr_t>(replacement));
		return true;
	}

	template <at_hook HookType>
	bool Create(std::string_view module, std::string_view targetName, void* replacement, std::string_view detourName = {})
	{
		auto hook = Setup<HookType>(module);
		return Create<HookType>(hook, targetName, replacement, detourName);
	}

	template <vmt_hook HookType>
	bool Create(VMTHook* vmtHook, const uint32_t index, void* newMethod, std::string_view name = "Unknown")
	{
		if (const auto originalMethod = vmtHook->Hook(index, newMethod); !originalMethod)
		{
			LOG_ERROR("Couldn't hook {}[{}] for {}, error: {}.", vmtHook->name, index, name, TinyHook::Utils::GetErrorMessage(originalMethod.error()));
			RETURN_FAIL(false)
		}

		LOG_INFO("Hooked virtual method {}[{}] -> {} (0x{:X}).", vmtHook->name, index, name, reinterpret_cast<uintptr_t>(newMethod));
		return true;
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

	template <safety_hook HookType>
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

	inline void UnhookEverything()
	{
		hooks.clear();
		tinyHooks<EATHook>.clear();
		tinyHooks<IATHook>.clear();
		tinyHooks<VMTHook>.clear();
	}

	template <tiny_hook HookType>
	void UnhookAll(const HookType* pHook)
	{
		for (const auto& [name, hook] : tinyHooks<HookType>)
		{
			if (hook.get() == pHook)
			{
				tinyHooks<HookType>.erase(name);
				return;
			}
		}
	}

	template <tiny_hook HookType>
	void UnhookAll(const std::string_view hookName)
	{
		if (tinyHooks<HookType>.contains(hookName))
		{
			tinyHooks<HookType>.erase(hookName);
		}
	}

	template <tiny_hook... Args>
	void UnhookAll(Args*... args)
	{
		(UnhookAll(args), ...);
	}

	template <function T>
	void Unhook(const T replacement)
	{
		std::unique_lock lock(hooking);
		if (hooks.contains(replacement))
		{
			hooks.erase(replacement);
		}
	}

	template <function... Args>
	void Unhook(Args... args)
	{
		(Unhook(args), ...);
	}

	inline void Unhook(const std::string_view hookName, const uint32_t index)
	{
		if (const auto it = tinyHooks<VMTHook>.find(hookName); it != tinyHooks<VMTHook>.end())
		{
			if (const auto& hook = it->second.get(); hook->IsHooked(index))
			{
				if (const auto result = hook->Unhook(index); !result)
				{
					LOG_ERROR("Couldn't unhook {}[{}], error: ", hookName, index, TinyHook::Utils::GetErrorMessage(result.error()));
				}
				return;
			}
			LOG_ERROR("{} isn't hooked at index {}.", hookName, index);
		}
		LOG_ERROR("Couldn't find a VMTHook for {}.", hookName);
	}

	template <at_hook HookType>
	void Unhook(const std::string_view functionName)
	{
		for (const auto& hook : tinyHooks<HookType> | std::views::values)
		{
			if (hook->IsHooked(functionName))
			{
				if (const auto result = hook->Unhook(functionName); !result)
				{
					LOG_ERROR("Couldn't unhook {}, error: ", functionName, TinyHook::Utils::GetErrorMessage(result.error()));
				}
				return;
			}
		}
		LOG_ERROR("Couldn't find a hook related to {}.", functionName);
	}
}

using HookVariant = std::variant<TinyHook::Original, std::reference_wrapper<InlineHook>>;

template <typename T>
concept hook_identifier = requires(T* t)
	{{ TinyHook::Manager::GetOriginal(t) } -> std::convertible_to<TinyHook::Original>;} || requires(T* t){{ HooksManager::GetOriginal<T>(t) };};

class OriginalFunc
{
public:
	template<hook_identifier HookType>
	explicit OriginalFunc(HookType* hookIdentifier)
	{
		if constexpr (requires { TinyHook::Manager::GetOriginal(hookIdentifier); })
		{
			auto tinyHook = TinyHook::Manager::GetOriginal(hookIdentifier);
			if (tinyHook.isValid())
			{
				hookVariant = tinyHook;
				return;
			}
		}

		if constexpr (requires { HooksManager::GetOriginal<HookType>(hookIdentifier); })
		{
			hookVariant = std::ref(HooksManager::GetOriginal<HookType>(hookIdentifier));
		}
	}

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return std::visit([]<typename T0>(const T0& hook) -> bool
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, TinyHook::Original>)
			{
				return hook.isValid();
			}
			else return true;
		}, hookVariant);
	}

	template <typename ReturnType, typename... Args>
	ReturnType call(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, TinyHook::Original>)
			{
				return hook.template call<ReturnType>(args...);
			}
			else return hook.get().template call<ReturnType>(args...);
		}, hookVariant);
	}

	template <typename ReturnType, typename... Args>
	ReturnType stdcall(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, TinyHook::Original>)
			{
				return hook.template stdcall<ReturnType>(args...);
			}
			else return hook.get().template stdcall<ReturnType>(args...);
		}, hookVariant);
	}

	template <typename ReturnType, typename... Args>
	ReturnType thiscall(Args... args) const
	{
		return std::visit([&]<typename T0>(const T0 & hook) -> ReturnType
		{
			if constexpr (std::is_same_v<std::decay_t<T0>, TinyHook::Original>)
			{
				return hook.template thiscall<ReturnType>(args...);
			}
			else return hook.get().template thiscall<ReturnType>(args...);
		}, hookVariant);
	}

private:
	HookVariant hookVariant;
};