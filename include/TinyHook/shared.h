#pragma once
#include <windows.h>
#include <unordered_map>

namespace TinyHook
{
    // Shared utility functions
    namespace Utils
    {
        template<typename T>
        void Patch(void* address, T value)
        {
            DWORD oldProtection;
            VirtualProtect(address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtection);
            *static_cast<T*>(address) = value;
            VirtualProtect(address, sizeof(T), oldProtection, &oldProtection);
        }

        template<typename T>
        void Patch(void** address, const size_t index, T* value)
        {
            const auto targetAddress = static_cast<void*>(&address[index]);
            Patch(targetAddress, static_cast<void*>(value));
        }
    }

    class OriginalFunction
    {
    public:
        OriginalFunction() = default;
        explicit OriginalFunction(const void* originalPtr) : pOriginal(originalPtr) {}
        explicit OriginalFunction(const uintptr_t originalPtr) : OriginalFunction(reinterpret_cast<void*>(originalPtr)) {}

        template<typename ReturnType, typename... Args>
        ReturnType call(Args... args) const {
            return reinterpret_cast<ReturnType(*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType, typename... Args>
        ReturnType fastcall(Args... args) const {
            return reinterpret_cast<ReturnType(__fastcall*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType, typename... Args>
        ReturnType stdcall(Args... args) const {
            return reinterpret_cast<ReturnType(__stdcall*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType, typename... Args>
        ReturnType thiscall(Args... args) const {
            return reinterpret_cast<ReturnType(__thiscall*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType, typename... Args>
        ReturnType vectorcall(Args... args) const {
            return reinterpret_cast<ReturnType(__vectorcall*)(Args...)>(pOriginal)(args...);
        }

        [[nodiscard]] bool isValid() const { return pOriginal != nullptr; }
        [[nodiscard]] const void* ptr() const { return pOriginal; }
        [[nodiscard]] uintptr_t ptr() { return reinterpret_cast<uintptr_t>(pOriginal); }

        explicit operator const void* () const { return ptr(); }
        explicit operator uintptr_t () const { return reinterpret_cast<uintptr_t>(ptr()); }
        explicit operator bool() const { return isValid(); }

    private:
        const void* pOriginal{ nullptr };
    };


    class Manager
    {
    public:
        static OriginalFunction& GetOriginal(const void* hookFunction)
        {
            if (const auto it = mHookChain.find(hookFunction); it != mHookChain.end())
            {
                return it->second;
            }
            return nullMethod;
        }

        static void RegisterHook(const void* hookFunction, const void* originalFunction)
        {
            mHookChain[hookFunction] = OriginalFunction(originalFunction);
        }

        static void RegisterHook(const void* hookFunction, const uintptr_t originalFunction)
        {
            RegisterHook(hookFunction, reinterpret_cast<void*>(originalFunction));
        }

        static void UnregisterHook(const void* hookFunction)
        {
            mHookChain.erase(hookFunction);
        }

        static void UnregisterHook(const uintptr_t hookFunction)
        {
            UnregisterHook(reinterpret_cast<void*>(hookFunction));
        }

        static void ClearAll()
        {
            mHookChain.clear();
        }

    private:
        inline static std::unordered_map<const void*, OriginalFunction> mHookChain;
        inline static OriginalFunction nullMethod;
    };
}

