#pragma once
#include "shared.h"
#include <unordered_map>
#include <string>
#include <windows.h>
#include <cstdint>
#include <utility>
#include <ranges>

namespace TinyHook
{
    class EATHook
    {
    public:
        explicit EATHook(void* hModule) : moduleBase(reinterpret_cast<uintptr_t>(hModule)) {}
        explicit EATHook(const char* moduleName) : EATHook(GetModuleHandleA(moduleName)) {}
        explicit EATHook(const wchar_t* moduleName) : EATHook(GetModuleHandleW(moduleName)) {}

        ~EATHook() { UnhookAll(); }

        bool Hook(const char* functionName, void* replacement);
        bool Unhook(const char* functionName);
        void UnhookAll();

        using HookedFunction = OriginalFunction;
        static OriginalFunction& GetOriginal(const void* replacement) { return Manager::GetOriginal(replacement); }

    private:
        uintptr_t moduleBase{};
        std::unordered_map<std::string, std::pair<uintptr_t*, uintptr_t>> mOriginalOffsets;

        std::pair<void*, uintptr_t*> FindEATFunction(const char* functionName) const;
    };

    inline bool EATHook::Hook(const char* functionName, void* replacement)
    {
        if (!moduleBase || !functionName || !replacement) return false;

        const auto [originalFunction, pAddressTable] = FindEATFunction(functionName);
        if (!originalFunction || !pAddressTable) return false;

        if (!mOriginalOffsets.contains(functionName))
        {
            mOriginalOffsets[functionName] = { pAddressTable, *pAddressTable };
        }

        Manager::RegisterHook(replacement, originalFunction);

        const uintptr_t newOffset = reinterpret_cast<uintptr_t>(replacement) - moduleBase;

        Utils::Patch(pAddressTable, reinterpret_cast<void*>(newOffset));
        return true;
    }

    inline bool EATHook::Unhook(const char* functionName)
    {
        const auto it = mOriginalOffsets.find(functionName);
        if (it == mOriginalOffsets.end()) return false;

        const auto [pRelativeOffset, originalValue] = it->second;
        const auto currentFunction = reinterpret_cast<void*>(moduleBase + *pRelativeOffset);

        Utils::Patch(pRelativeOffset, reinterpret_cast<void*>(originalValue));

        Manager::UnregisterHook(currentFunction);
        mOriginalOffsets.erase(it);
        return true;
    }

    inline void EATHook::UnhookAll()
    {
        for (const auto& hookInfo : mOriginalOffsets | std::views::values)
        {
            const auto [pRelativeOffset, originalValue] = hookInfo;
            const uintptr_t currentOffset = *pRelativeOffset;

            if (currentOffset != originalValue)
            {
                const auto currentFunction = reinterpret_cast<void*>(moduleBase + currentOffset);
                Manager::UnregisterHook(currentFunction);
                Utils::Patch(pRelativeOffset, reinterpret_cast<void*>(originalValue));
            }
        }
        mOriginalOffsets.clear();
    }

    inline std::pair<void*, uintptr_t*> EATHook::FindEATFunction(const char* functionName) const
    {
        if (!moduleBase || !functionName) return { nullptr, nullptr };

        const auto dosHeaders = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
        const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + dosHeaders->e_lfanew);
        const auto [VirtualAddress, Size] = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

        const auto pExportDirectory = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(reinterpret_cast<PCHAR>(dosHeaders) + VirtualAddress);
        const auto pAddressTable = reinterpret_cast<uintptr_t*>(moduleBase + pExportDirectory->AddressOfFunctions);
        const uintptr_t* pNameTable = reinterpret_cast<uintptr_t*>(moduleBase + pExportDirectory->AddressOfNames);
        const uint16_t* pOrdinalTable = reinterpret_cast<uint16_t*>(moduleBase + pExportDirectory->AddressOfNameOrdinals);

        for (uintptr_t i = 0; i < pExportDirectory->NumberOfNames; i++)
        {
            const char* exportedName = reinterpret_cast<char*>(moduleBase + pNameTable[i]);
            if (_stricmp(exportedName, functionName) == 0)
            {
                const uint16_t ordinal = pOrdinalTable[i];
                uintptr_t* pRelativeOffset = &pAddressTable[ordinal];
                const auto originalFunction = reinterpret_cast<void*>(moduleBase + *pRelativeOffset);
                return { originalFunction, pRelativeOffset };
            }
        }
        return { nullptr, nullptr };
    }
}
