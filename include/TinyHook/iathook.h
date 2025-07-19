#pragma once
#include "shared.h"
#include <string>
#include <ranges>

namespace TinyHook
{
    class IATHook
    {
    public:
        explicit IATHook() : moduleBase(reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr))) {}
        explicit IATHook(void* hModule) : moduleBase(reinterpret_cast<uintptr_t>(hModule)) {}
        explicit IATHook(const char* moduleName) : IATHook(GetModuleHandleA(moduleName)) {}
        explicit IATHook(const wchar_t* moduleName) : IATHook(GetModuleHandleW(moduleName)) {}
        ~IATHook() { UnhookAll(); }

        bool Hook(const char* functionName, void* newFunction)
        {
            if (!functionName || !newFunction) return false;

            uintptr_t* pFunctionAddress = FindIATFunction(functionName);
            if (!pFunctionAddress) return false;

            const auto originalFunction = *pFunctionAddress;
            if (!mOriginalFunctions.contains(functionName))
            {
                mOriginalFunctions[functionName] = { pFunctionAddress, originalFunction };
            }

            Manager::RegisterHook(newFunction, originalFunction);
            Utils::Patch(pFunctionAddress, newFunction);
            return true;
        }

        bool Unhook(const char* functionName)
        {
            const auto it = mOriginalFunctions.find(functionName);
            if (it == mOriginalFunctions.end()) return false;

            const auto [pFunctionAddress, originalAddress] = it->second;
            const auto currentFunction = *pFunctionAddress;

            Utils::Patch(pFunctionAddress, originalAddress);
            Manager::UnregisterHook(currentFunction);
            mOriginalFunctions.erase(it);
            return true;
        }
        void UnhookAll()
        {
            for (const auto& hookInfo : mOriginalFunctions | std::views::values)
            {
                const auto [pFunctionAddress, originalFunction] = hookInfo;
                const auto currentFunction = *pFunctionAddress;
                if (currentFunction != originalFunction)
                {
                    Utils::Patch(pFunctionAddress, originalFunction);
                    Manager::UnregisterHook(currentFunction);
                }
            }
            mOriginalFunctions.clear();
        }

        static OriginalFunction& GetOriginal(const void* hookFunction) { return Manager::GetOriginal(hookFunction); }

    private:
        uintptr_t moduleBase;
        std::unordered_map<std::string, std::pair<uintptr_t*, uintptr_t>> mOriginalFunctions; // name -> (original_ptr, hook_address)

        uintptr_t* FindIATFunction(const char* functionName) const
        {
	        if (!moduleBase || !functionName) return nullptr;

	        const auto dosHeaders = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
	        const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + dosHeaders->e_lfanew);

	        const auto [VirtualAddress, Size] = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	        auto importDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(VirtualAddress + moduleBase);

	        while (importDescriptor->Name)
	        {
		        auto firstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(moduleBase + importDescriptor->FirstThunk);
		        auto originalFirstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(moduleBase + importDescriptor->OriginalFirstThunk);

		        while (originalFirstThunk->u1.AddressOfData)
		        {
			        if (!(originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG))
			        {
				        const auto pImport = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(moduleBase + originalFirstThunk->u1.AddressOfData);

				        if (pImport && _stricmp(pImport->Name, functionName) == 0)
				        {
					        return reinterpret_cast<uintptr_t*>(&firstThunk->u1.Function);
				        }
			        }
			        ++originalFirstThunk;
			        ++firstThunk;
		        }
		        importDescriptor++;
	        }
	        return nullptr;
        }
    };
}