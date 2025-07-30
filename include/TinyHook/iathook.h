#pragma once
#include "shared.h"
#include <ranges>

namespace TinyHook
{
    class IATHook
    {
    public:
        std::string name;

        explicit IATHook() : moduleBase(reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr))) {}
        explicit IATHook(void* hModule, const std::string_view name) : name(name), moduleBase(reinterpret_cast<uintptr_t>(hModule)) {}
        explicit IATHook(void* hModule) : IATHook(hModule, Utils::GetModuleFilename(hModule)) {}
        explicit IATHook(const char* moduleName) : IATHook(GetModuleHandleA(moduleName), moduleName) {}
        explicit IATHook(const wchar_t* moduleName) : IATHook(GetModuleHandleW(moduleName), Utils::ConvertToString(moduleName)) {}

    	~IATHook() { UnhookAll(); }

        std::expected<bool, Error> Hook(const char* functionName, void* newFunction)
        {
            if (!functionName) return std::unexpected(Error::FunctionNotFound);
            if (!newFunction) return std::unexpected(Error::InvalidDetour);
				
            const auto found = FindIATFunction(functionName);
            if (!found) return std::unexpected(found.error());

            const auto pOriginal = found.value();

            Manager::RegisterHook(newFunction, *pOriginal);

            if (const auto result = Utils::Patch(pOriginal, newFunction); !result)
            {
            	return std::unexpected(result.error());
            }
            return true;
        }

        std::expected<bool, Error> Unhook(const std::string& functionName)
        {
            const auto it = mOriginalFunctions.find(functionName);
            if (it == mOriginalFunctions.end()) return std::unexpected(Error::NotHooked);

            const auto [pFunctionAddress, originalAddress] = it->second;
            const auto currentFunction = *pFunctionAddress;

            if (const auto result = Utils::Patch(pFunctionAddress, originalAddress); !result)
            {
	            return std::unexpected(result.error());
            }

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
                    const auto result = Utils::Patch(pFunctionAddress, originalFunction);
                    Manager::UnregisterHook(currentFunction);
                }
            }
            mOriginalFunctions.clear();
        }

        [[nodiscard]] size_t GetHookCount() const noexcept
        {
            return mOriginalFunctions.size();
        }

        [[nodiscard]] bool IsHooked(const std::string& function) const noexcept
        {
            return mOriginalFunctions.contains(function);
        }

		// Returns the original function pointer if the function is hooked, otherwise returns an error.
        [[nodiscard]] std::expected<uintptr_t*, Error> GetOriginal(const std::string& function) const noexcept
        {
            if (const auto it = mOriginalFunctions.find(function); it != mOriginalFunctions.end())
            {
                return it->second.first;
            }
			return std::unexpected(Error::NotHooked);
        }

        static Original& GetOriginal(const void* hookFunction) { return Manager::GetOriginal(hookFunction); }

    private:
        uintptr_t moduleBase;
        std::unordered_map<std::string, std::pair<uintptr_t*, uintptr_t>> mOriginalFunctions; // name -> (original_ptr, hook_address)

        std::expected<uintptr_t*, Error> FindIATFunction(const char* functionName)
        {
	        if (!moduleBase) return std::unexpected(Error::InvalidModule);
	        if (!functionName) return  std::unexpected(Error::FunctionNotFound);

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
                            mOriginalFunctions.try_emplace(functionName, std::pair(&firstThunk->u1.Function, firstThunk->u1.Function));
					        return reinterpret_cast<uintptr_t*>(&firstThunk->u1.Function);
				        }
			        }
			        ++originalFirstThunk;
			        ++firstThunk;
		        }
		        importDescriptor++;
	        }
	        return std::unexpected(Error::FunctionNotFound);
        }
    };
}