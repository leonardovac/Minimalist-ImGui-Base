#pragma once
#include <windows.h>
#include <unordered_map>
#include <type_traits>
#include <expected>
#include <filesystem>
#include <string_view>

namespace TinyHook
{
    template<typename T>
    concept value = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

    template<typename T>
    concept pointer = std::is_pointer_v<T>;

    enum class Error : std::uint8_t
    {
        InvalidAddress,
        InvalidDetour,
        InvalidModule,
		FunctionNotFound,
        AlreadyHooked,
        NotHooked,
        IndexOutOfBounds,
        ProtectionError
    };

    namespace Utils
    {
        template<typename T>
        constexpr std::expected<void, Error> Patch(void* address, T value)
        {
            if (!address) return std::unexpected(Error::InvalidAddress);
            

            DWORD oldProtection;
            if (!VirtualProtect(address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtection))
        	{
                return std::unexpected(Error::ProtectionError);
            }

            *static_cast<T*>(address) = value;

            if (!VirtualProtect(address, sizeof(T), oldProtection, &oldProtection))
            {
                return std::unexpected(Error::ProtectionError);
            }

            return {};
        }

        template<pointer T>
        constexpr std::expected<void, Error> Patch(void** address, const size_t index, T value)
        {
            if (!address) return std::unexpected(Error::InvalidAddress);

            return Patch(static_cast<void*>(&address[index]), static_cast<void*>(value));
        }

        [[nodiscard]] constexpr std::string_view GetErrorMessage(const Error error) noexcept
        {
            using enum Error;
            switch (error)
            {
            case InvalidAddress:    return "Invalid memory address";
            case InvalidDetour:    return "Invalid detour address";
            case InvalidModule:   return "Module is not loaded";
            case ProtectionError:       return "Memory protection (VirtualProtect) failed";
            case FunctionNotFound:       return "Function not found on address table";
            case AlreadyHooked:     return "Hook already exists";
            case IndexOutOfBounds:     return "Index passed was bigger than table size";
            case NotHooked:         return "Not currently hooked";
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
            	if (const DWORD result = GetModuleFileNameW(static_cast<HMODULE>(hModule), buffer.data(), MAX_PATH); result == 0)
            	{
            		const std::filesystem::path modulePath{ buffer.data() };
            		return modulePath.filename().string(); // Just the filename
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