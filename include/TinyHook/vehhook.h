#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <Windows.h>

#include "shared.h"

#define PAGE_MASK (~(4096 - 1))
#define GetPage(Ptr) ((uintptr_t)(Ptr) & PAGE_MASK)

namespace TinyHook
{
    class VEHHook
    {
    public:
        static VEHHook& GetInstance()
        {
            static VEHHook instance;
            return instance;
        }

        VEHHook(const VEHHook&) = delete;
        VEHHook& operator=(const VEHHook&) = delete;
        VEHHook(VEHHook&&) = delete;
        VEHHook& operator=(VEHHook&&) = delete;

        template<callback CallbackFunc>
        std::expected<bool, Error> Hook(void* address, CallbackFunc callbackFunc)
        {
            if (!address) return std::unexpected(Error::InvalidAddress);
            if (!callbackFunc) return std::unexpected(Error::InvalidDetour);

            std::unique_lock lock(hooksMutex);

            if (hooks.contains(address)) return std::unexpected(Error::AlreadyHooked);

            // Store callback as std::function to handle both signatures
            hooks[address] = HookInfo{std::function<void(CONTEXT*)>(callbackFunc), GetPage(address)};

            return GuardPage(address);
        }

        std::expected<bool, Error> Unhook(const void* address)
        {
            if (!address) return std::unexpected(Error::InvalidAddress);

            std::unique_lock lock(hooksMutex);

            const auto it = hooks.find(const_cast<void*>(address));
            if (it == hooks.end()) return std::unexpected(Error::NotHooked);
            hooks.erase(it);

            return true;
        }

        void UnhookAll()
        {
            std::unique_lock lock(hooksMutex);
            hooks.clear();
        }

    private:
        struct HookInfo
        {
            std::function<void(CONTEXT*)> callback;
            uintptr_t pageBase = NULL;

            HookInfo() = default;
            HookInfo(std::function<void(CONTEXT*)> cb, const uintptr_t page) : callback(std::move(cb)), pageBase(page) {}

            // Assignment operator
            HookInfo& operator=(const HookInfo& other)
            {
                if (this != &other)
                {
                    callback = other.callback;
                    pageBase = other.pageBase;
                }
                return *this;
            }
        };

        VEHHook()
        {
            if (!pVEHHandle)
            {
                pVEHHandle = AddVectoredExceptionHandler(1, VectoredHandler);
            }
        }

        ~VEHHook()
        {
            UnhookAll();
            if (pVEHHandle)
            {
                RemoveVectoredExceptionHandler(pVEHHandle);
                pVEHHandle = nullptr;
            }
        }

        static bool GuardPage(const void* address)
        {
            DWORD oldProtect;
            MEMORY_BASIC_INFORMATION mbi{};

            if (!VirtualQuery(address, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) || mbi.Protect & PAGE_GUARD) return false;

            return VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect | PAGE_GUARD, &oldProtect) != 0;
        }

        static LONG WINAPI VectoredHandler(const PEXCEPTION_POINTERS pExceptionInfo)
        {
            auto& instance = GetInstance();
            thread_local const void* lastAddress = nullptr;

            if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_GUARD_PAGE)
            {
                std::shared_lock lock(instance.hooksMutex);
#ifdef _WIN64
                const auto nextInstruction = reinterpret_cast<void*>(pExceptionInfo->ContextRecord->Rip);
#else 
                const auto nextInstruction = reinterpret_cast<void*>(pExceptionInfo->ContextRecord->Eip);
#endif
                const auto virtualAddress = reinterpret_cast<void*>(pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);

                for (const auto& [hookAddress, hookInfo] : instance.hooks)
                {
                    if (GetPage(virtualAddress) != hookInfo.pageBase) continue;

                    lastAddress = virtualAddress;
                    pExceptionInfo->ContextRecord->EFlags |= PAGE_GUARD;

                    if (nextInstruction == hookAddress)
                    {
                        if (hookInfo.callback)
                        {
                            hookInfo.callback(pExceptionInfo->ContextRecord);
                        }
                        break;
                    }
                }

                return EXCEPTION_CONTINUE_EXECUTION;
            }

            if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
            {
                std::shared_lock lock(instance.hooksMutex);

                if (lastAddress)
                {
                    GuardPage(lastAddress);
                    lastAddress = nullptr;
                }

                return EXCEPTION_CONTINUE_EXECUTION;
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        std::unordered_map<void*, HookInfo> hooks;
        std::shared_mutex hooksMutex;
        static inline PVOID pVEHHandle = nullptr;
    };
}