#pragma once
#include "shared.h"
#include <unordered_map>
#include <windows.h>
#include <cstdint>
#include <utility>
#include <ranges>

namespace TinyHook
{
    class EATHook
    {
    public:
        std::string name;

        template<address T>
        explicit EATHook(T hModule, const std::string_view name) : name(name), moduleBase(reinterpret_cast<uintptr_t>(hModule)) {}
        template<address T>
        explicit EATHook(T hModule) : EATHook(hModule, Utils::GetModuleFilename(reinterpret_cast<void*>(hModule))) {}

        explicit EATHook(const char* moduleName) : EATHook(GetModuleHandleA(moduleName), moduleName) {}
        explicit EATHook(const wchar_t* moduleName) : EATHook(GetModuleHandleW(moduleName), Utils::ConvertToString(moduleName)) {}

        ~EATHook() { UnhookAll(); }

        std::expected<bool, Error> Hook(const char* functionName, void* replacement)
        {
            if (!replacement) return std::unexpected(Error::InvalidDetour);

            const auto found = FindEATFunction(functionName);
            if (!found) return std::unexpected(found.error());

            const auto [originalFunction, pOffset] = found.value();
            
            const uintptr_t newOffset = reinterpret_cast<uintptr_t>(replacement) - moduleBase;
            if (const auto patch = Utils::Patch(pOffset, reinterpret_cast<void*>(newOffset)); !patch)
            {
                return std::unexpected(patch.error());
            }
            Manager::RegisterHook(replacement, originalFunction);
            return true;
        }

        std::expected<bool, Error> Unhook(const std::string_view functionName)
        {
            const auto it = mOriginalOffsets.find(std::string{ functionName });
            if (it == mOriginalOffsets.end()) return std::unexpected(Error::NotHooked);

            const auto [pRelativeOffset, originalValue] = it->second;
            const auto currentFunction = reinterpret_cast<void*>(moduleBase + *pRelativeOffset);

            if (const auto result = Utils::Patch(pRelativeOffset, reinterpret_cast<void*>(originalValue)); !result)
            {
                return std::unexpected(result.error());
            }

            Manager::UnregisterHook(currentFunction);
            mOriginalOffsets.erase(it);
            return true;
        }

        void UnhookAll()
        {
            for (const auto& hookInfo : mOriginalOffsets | std::views::values)
            {
                const auto [pRelativeOffset, originalValue] = hookInfo;
                const uintptr_t currentOffset = *pRelativeOffset;

                if (currentOffset != originalValue)
                {
                    const auto currentFunction = reinterpret_cast<void*>(moduleBase + currentOffset);
                    Manager::UnregisterHook(currentFunction);
                    const auto result = Utils::Patch(pRelativeOffset, reinterpret_cast<void*>(originalValue));
                }
            }
            mOriginalOffsets.clear();
        }

        [[nodiscard]] size_t GetHookCount() const noexcept
        {
            return mOriginalOffsets.size();
        }

        [[nodiscard]] bool IsHooked(const std::string_view function) const noexcept
        {
            return mOriginalOffsets.contains(std::string{ function });
        }

        // Returns a pointer to the original offset if the function is hooked, otherwise returns an error.
        [[nodiscard]] std::expected<uintptr_t*, Error> GetOriginal(const std::string_view function) const noexcept
        {
            if (const auto it = mOriginalOffsets.find(std::string{ function }); it != mOriginalOffsets.end())
            {
                return it->second.first;
            }
            return std::unexpected(Error::NotHooked);
        }

        static Original& GetOriginal(const void* replacement) { return Manager::GetOriginal(replacement); }

    private:
        uintptr_t moduleBase{};
        std::unordered_map<std::string, std::pair<uintptr_t*, uintptr_t>> mOriginalOffsets;

        std::expected <std::pair<void*, uintptr_t*>, Error> FindEATFunction(const char* functionName)
        {
            if (!moduleBase) return std::unexpected(Error::InvalidModule);
            if (!functionName) return std::unexpected(Error::FunctionNotFound);

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
                	mOriginalOffsets.try_emplace(functionName, std::pair(pRelativeOffset, *pRelativeOffset));
                    return std::pair(originalFunction, pRelativeOffset);
                }
            }
            return std::unexpected(Error::FunctionNotFound);
        }
    };
}
