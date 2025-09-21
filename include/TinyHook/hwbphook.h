#pragma once
#include <array>
#include <functional>
#include <tlhelp32.h>
#include <Windows.h>

#include "shared.h"

namespace TinyHook
{
    enum class AccessType : std::uint8_t
    {
        Execute = 0,
        Write = 1,
        ReadWrite = 3
    };

    enum class Size : std::uint8_t
    {
        Byte = 0,
        Word = 1,
        DWord = 3,
        QWord = 2
    };

    class HWBPHook
    {
    public:
        static HWBPHook& GetInstance()
        {
            static HWBPHook instance;
            return instance;
        }

        HWBPHook(const HWBPHook&) = delete;
        HWBPHook& operator=(const HWBPHook&) = delete;

        template <callback Callback>
        std::expected<bool, Error> Hook(void* address, Callback callback, const AccessType type = AccessType::Execute, const Size size = Size::Byte)
        {
            if (!address) return std::unexpected(Error::InvalidAddress);

            const int drIndex = FindFreeDebugRegister();
            if (drIndex == -1) return std::unexpected(Error::IndexOutOfBounds);

            // Store the breakpoint info first
            breakpoints[drIndex] = {.address = address, .callback = callback, .type = type, .size = size };

            // Apply to ALL threads in the process
            if (!ApplyToAllThreads([&](const HANDLE hThread, CONTEXT& ctx)
            {
	            SetDebugRegister(ctx, drIndex, address, type, size);
	            SetThreadContext(hThread, &ctx);
            }))
            {
	            breakpoints[drIndex] = {}; // Clear on failure
	            return std::unexpected(Error::ProtectionError);
            }

            // Also apply to current thread
            CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (const HANDLE currentThread = GetCurrentThread(); GetThreadContext(currentThread, &context))
            {
                SetDebugRegister(context, drIndex, address, type, size);
                SetThreadContext(currentThread, &context);
            }

            return true;
        }

        std::expected<bool, Error> Unhook(const void* address)
        {
            // Find and remove breakpoint
            for (int i = 0; i < 4; ++i)
            {
                if (breakpoints[i].address == address)
                {
                    breakpoints[i] = {}; // Clear

                    // Clear from current thread
                    CONTEXT context{};
                    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                    const HANDLE currentThread = GetCurrentThread();

                    if (GetThreadContext(currentThread, &context))
                    {
                        ClearDebugRegister(context, i);
                        SetThreadContext(currentThread, &context);
                    }
                    return true;
                }
            }
            return std::unexpected(Error::NotHooked);
        }

    private:
        struct BreakpointInfo
        {
            void* address = nullptr;
            std::function<void(CONTEXT*)> callback;
            AccessType type = AccessType::Execute;
            Size size = Size::Byte;
        };

        HWBPHook()
        {
            if (!pVEHHandle)
            {
                pVEHHandle = AddVectoredExceptionHandler(1, VectoredHandler);
            }
        }

        ~HWBPHook()
        {
            if (pVEHHandle)
            {
                RemoveVectoredExceptionHandler(pVEHHandle);
            }
        }

        static bool ApplyToAllThreads(const std::function<void(HANDLE, CONTEXT&)>& action)
        {
            const HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (hSnapshot == INVALID_HANDLE_VALUE) return false;

            THREADENTRY32 te{};
            te.dwSize = sizeof(THREADENTRY32);
            const DWORD currentProcessId = GetCurrentProcessId();
            const DWORD currentThreadId = GetCurrentThreadId();

            bool success = true;

            if (Thread32First(hSnapshot, &te))
            {
                do
                {
                    if (te.th32OwnerProcessID == currentProcessId && te.th32ThreadID != currentThreadId)
                    {
                        if (const HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID))
                        {
                            if (const DWORD suspendCount = SuspendThread(hThread); std::cmp_not_equal(suspendCount, -1))
                            {
                                CONTEXT context{};
                                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

                                if (GetThreadContext(hThread, &context))
                                {
                                    action(hThread, context);
                                }
                                else
                                {
                                    success = false;
                                }

                                ResumeThread(hThread);
                            }
                            else
                            {
                                success = false;
                            }

                            CloseHandle(hThread);
                        }
                    }
                } while (Thread32Next(hSnapshot, &te));
            }

            CloseHandle(hSnapshot);
            return success;
        }

        static LONG WINAPI VectoredHandler(const PEXCEPTION_POINTERS pExceptionInfo)
        {
            if (pExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;

            const auto& instance = GetInstance();

            for (int i = 0; i < 4; ++i)
            {
                if (pExceptionInfo->ContextRecord->Dr6 & 1ULL << i && instance.breakpoints[i].address)
                {
                    pExceptionInfo->ContextRecord->Dr6 &= ~(1ULL << i);

                    if (instance.breakpoints[i].callback)
                    {
                        instance.breakpoints[i].callback(pExceptionInfo->ContextRecord);
                    }

                    pExceptionInfo->ContextRecord->EFlags |= 1 << 16;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        int FindFreeDebugRegister() const
        {
            for (int i = 0; i < 4; ++i)
            {
                if (!breakpoints[i].address) return i;
            }
            return -1;
        }

        static void SetDebugRegister(CONTEXT& context, const int index, void* address, AccessType type, Size size)
        {
            const auto addr = reinterpret_cast<DWORD_PTR>(address);

            switch (index)
            {
            case 0: context.Dr0 = addr; break;
            case 1: context.Dr1 = addr; break;
            case 2: context.Dr2 = addr; break;
            case 3: context.Dr3 = addr; break;
            default: break;
            }

            DWORD_PTR dr7 = context.Dr7;
            dr7 |= 1ULL << (index * 2); // Enable breakpoint

            const int configShift = 16 + index * 4;
            dr7 &= ~(0xFULL << configShift);
            dr7 |= static_cast<DWORD>(type) << configShift;
            dr7 |= static_cast<DWORD>(size) << (configShift + 2);

            context.Dr7 = dr7;
        }

        static void ClearDebugRegister(CONTEXT& context, const int index)
        {
            switch (index)
            {
            case 0: context.Dr0 = 0; break;
            case 1: context.Dr1 = 0; break;
            case 2: context.Dr2 = 0; break;
            case 3: context.Dr3 = 0; break;
            default: break;
            }

            DWORD_PTR dr7 = context.Dr7;
            dr7 &= ~(1ULL << (index * 2));
            dr7 &= ~(0xFULL << (16 + index * 4));
            context.Dr7 = dr7;
        }

        std::array<BreakpointInfo, 4> breakpoints{};
        static inline PVOID pVEHHandle = nullptr;
    };
}