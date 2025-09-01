#pragma once
#include <expected>
#include <filesystem>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <windows.h>

namespace TinyHook
{
	template<typename T>
	concept void_convertible = requires(T t) { static_cast<const void*>(t); };

	template<typename T>
	concept value = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

	template<typename T>
	concept address = void_convertible || std::is_same_v<T, void*> || std::is_same_v<T, uintptr_t>  || std::is_same_v<T, DWORD> || std::is_same_v<T, HMODULE>;

	enum class Error : std::uint8_t
	{
		AlreadyHooked,
		FunctionNotFound,
		IndexOutOfBounds,
		InvalidAddress,
		InvalidDetour,
		InvalidModule,
		NotHooked,
		NotInitialized,
		ProtectionError
	};

	namespace Utils
	{
		template<address T>
		constexpr std::expected<void, Error> Patch(void* pAddress, T value)
		{
			if (!pAddress) return std::unexpected(Error::InvalidAddress);

			MEMORY_BASIC_INFORMATION mbi;
			if (!VirtualQuery(pAddress, &mbi, sizeof(mbi)))
			{
				return std::unexpected(Error::InvalidAddress);
			}

			DWORD oldProtection{ 0 };
			bool bProtectionChanged{ false };

			// Check if it's writable
			if ((mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) == 0)
			{
				// Change protection
				if (!VirtualProtect(pAddress, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtection))
				{
					return std::unexpected(Error::ProtectionError);
				}
				bProtectionChanged = true;
			}

			// Change value
			*static_cast<T*>(pAddress) = value;

			// Restore original protection
			if (bProtectionChanged)
			{
				if (!VirtualProtect(pAddress, sizeof(T), oldProtection, &oldProtection))
				{
					return std::unexpected(Error::ProtectionError);
				}
			}

			return {};
		}

		template<address T>
		constexpr std::expected<void, Error> Patch(void** ppAddress, const size_t index, T value)
		{
			if (!ppAddress) return std::unexpected(Error::InvalidAddress);

			return Patch(static_cast<void*>(&ppAddress[index]), static_cast<void*>(value));
		}

		[[nodiscard]] constexpr std::string_view GetErrorMessage(const Error error) noexcept
		{
			using enum Error;
			switch (error)
			{
			case AlreadyHooked: return "Hook already exists";
			case FunctionNotFound: return "Function not found on address table";
			case IndexOutOfBounds: return "Index passed was bigger than table size";
			case InvalidAddress: return "Invalid memory address";
			case InvalidDetour: return "Invalid detour address";
			case InvalidModule: return "Module is not loaded";
			case NotHooked: return "Not currently hooked";
			case NotInitialized: return "Not initialized";
			case ProtectionError: return "Memory protection (VirtualProtect) failed";
			}
			return "Unknown error";
		}

		inline std::string ConvertToString(const std::wstring_view wideView)
		{
			if (wideView.empty()) return {};

			const int size = WideCharToMultiByte(CP_UTF8, 0, wideView.data(), static_cast<int>(wideView.size()), nullptr, 0, nullptr, nullptr);
			std::string result(size, 0);
			WideCharToMultiByte(CP_UTF8, 0, wideView.data(), static_cast<int>(wideView.size()), result.data(), size,nullptr, nullptr);
			return result;
		}

		template<typename T>
		std::string GetModuleFilename(const T hModule)
		{
			if (hModule)
			{
				std::array<wchar_t, MAX_PATH> buffer{};

				if (const DWORD result = GetModuleFileNameW(static_cast<HMODULE>(hModule), buffer.data(), MAX_PATH); result != 0 && result < MAX_PATH)
				{
					const std::filesystem::path modulePath{ buffer.data() };
					return modulePath.filename().string();
				}
			}
			return "Unknown";
		}
	}

	class Original
	{
	public:
		constexpr Original() noexcept = default;

		constexpr explicit Original(const void* originalPtr) noexcept : pOriginal(originalPtr) {}
		constexpr explicit Original(const uintptr_t originalPtr) noexcept : Original(reinterpret_cast<const void*>(originalPtr)) {}

		template<typename ReturnType, typename... Args>
		constexpr ReturnType call(Args... args) const noexcept(noexcept(std::declval<ReturnType(*)(Args...)>()(args...)))
		{
			return reinterpret_cast<ReturnType(*)(Args...)>(pOriginal)(args...);
		}

		template<typename ReturnType, typename... Args>
		constexpr ReturnType fastcall(Args... args) const noexcept(noexcept(std::declval<ReturnType(__fastcall*)(Args...)>()(args...)))
		{
			return reinterpret_cast<ReturnType(__fastcall*)(Args...)>(pOriginal)(args...);
		}

		template<typename ReturnType, typename... Args>
		constexpr ReturnType stdcall(Args... args) const noexcept(noexcept(std::declval<ReturnType(__stdcall*)(Args...)>()(args...)))
		{
			return reinterpret_cast<ReturnType(__stdcall*)(Args...)>(pOriginal)(args...);
		}

		template<typename ReturnType, typename... Args>
		constexpr ReturnType thiscall(Args... args) const noexcept(noexcept(std::declval<ReturnType(__thiscall*)(Args...)>()(args...)))
		{
			return reinterpret_cast<ReturnType(__thiscall*)(Args...)>(pOriginal)(args...);
		}

		template<typename ReturnType, typename... Args>
		constexpr ReturnType vectorcall(Args... args) const noexcept(noexcept(std::declval<ReturnType(__vectorcall*)(Args...)>()(args...)))
		{
			return reinterpret_cast<ReturnType(__vectorcall*)(Args...)>(pOriginal)(args...);
		}

		[[nodiscard]] bool isValid() const noexcept { return pOriginal != nullptr; }
		[[nodiscard]] const void* ptr() const noexcept { return pOriginal; }
		[[nodiscard]] uintptr_t address() const noexcept { return reinterpret_cast<uintptr_t>(pOriginal); }

		explicit operator const void* () const noexcept { return ptr(); }
		explicit operator uintptr_t() const noexcept { return address(); }
		explicit operator bool() const noexcept { return isValid(); }

	private:
		const void* pOriginal = nullptr;
	};

	class Manager
	{
	public:
		[[nodiscard]] static Original& GetOriginal(const void* hookFunction) noexcept
		{
			if (const auto it = mHookChain.find(hookFunction); it != mHookChain.end())
			{
				return it->second;
			}
			return nullMethod;
		}

		static void RegisterHook(const void* hookFunction, const void* originalFunction) noexcept
		{
			mHookChain.insert_or_assign(hookFunction, Original(originalFunction));
		}

		static void RegisterHook(const void* hookFunction, const uintptr_t originalFunction) noexcept
		{
			RegisterHook(hookFunction, reinterpret_cast<const void*>(originalFunction));
		}

		static bool UnregisterHook(const void* hookFunction) noexcept
		{
			return mHookChain.erase(hookFunction) > 0;
		}

		static bool UnregisterHook(const uintptr_t hookFunction) noexcept
		{
			return UnregisterHook(reinterpret_cast<const void*>(hookFunction));
		}

		static void ClearAll() noexcept
		{
			mHookChain.clear();
		}

		[[nodiscard]] static size_t GetHookCount() noexcept
		{
			return mHookChain.size();
		}

		[[nodiscard]] static bool IsHookRegistered(const void* hookFunction) noexcept
		{
			return mHookChain.contains(hookFunction);
		}

		[[nodiscard]] static bool IsHookRegistered(const uintptr_t hookFunction) noexcept
		{
			return IsHookRegistered(reinterpret_cast<const void*>(hookFunction));
		}

	private:
		inline static std::unordered_map<const void*, Original> mHookChain;
		inline static Original nullMethod;
	};
}