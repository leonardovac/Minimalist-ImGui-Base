#pragma once
#include <unordered_map>
#include "shared.h"

namespace TinyHook
{
    class VMTHook
    {
    public:
        std::string name;

        VMTHook() = default;

        template<typename T>
        explicit VMTHook(T* vTable, const std::string_view hookName = "Unknown") : name(hookName), pVTable(reinterpret_cast<void**>(vTable)), tableSize(LookupVTableForSize()) {}

        ~VMTHook() { UnhookAll(); }

        constexpr std::expected<void*, Error> Hook(const uint32_t index, void* newMethod)
        {
            if (!newMethod) return std::unexpected(Error::InvalidDetour);
            if (index >= tableSize) return std::unexpected(Error::IndexOutOfBounds);

            const auto currentMethod = pVTable[index];
            mOriginalMethods.try_emplace(index, currentMethod);

            Manager::RegisterHook(newMethod, currentMethod);
            if (const auto result = Utils::Patch(pVTable, index, newMethod); !result)
            {
	            return std::unexpected(result.error());
            }
            return currentMethod;
        }

        constexpr std::expected<bool, Error> Unhook(const uint32_t index)
        {
            if (index >= tableSize) return std::unexpected(Error::IndexOutOfBounds);
            if (!mOriginalMethods.contains(index)) return std::unexpected(Error::NotHooked);

            const auto currentMethod = pVTable[index];
            const auto originalMethod = mOriginalMethods[index];

            if (const auto result = Utils::Patch(pVTable, index, originalMethod); !result)
            {
            	return std::unexpected(result.error());
            }
            Manager::UnregisterHook(currentMethod);
            mOriginalMethods.erase(index);
            return true;
        }

        void UnhookAll()
        {
            for (const auto& [index, originalMethod] : mOriginalMethods)
            {
                if (const auto currentMethod = pVTable[index]; currentMethod != originalMethod)
                {
                    const auto result = Utils::Patch(pVTable, index, originalMethod);
                    Manager::UnregisterHook(currentMethod);
                }
            }
            mOriginalMethods.clear();
        }

        [[nodiscard]] size_t GetHookCount() const noexcept
        {
            return mOriginalMethods.size();
        }

        [[nodiscard]] bool IsHooked(const uint32_t index) const noexcept
        {
            return mOriginalMethods.contains(index);
        }

        [[nodiscard]] std::expected<void*, Error> GetOriginal(const uint32_t index) const noexcept
        {
            if (const auto it = mOriginalMethods.find(index); it != mOriginalMethods.end())
            {
                return it->second;
            }
            return std::unexpected(Error::NotHooked);
        }

        [[nodiscard]] constexpr uint32_t GetTableSize() const noexcept
        {
            return tableSize;
        }

        static Original& GetOriginal(const void* hookFunction) { return Manager::GetOriginal(hookFunction); }

    private:
        void** pVTable;
        uint32_t tableSize;
        std::unordered_map<uint32_t, void*> mOriginalMethods;

        [[nodiscard]] uint32_t LookupVTableForSize() const
        {
            uint32_t index = 0;
            MEMORY_BASIC_INFORMATION mbiBuffer = {};

            if (pVTable)
	        {
		        while (VirtualQuery(pVTable[index], &mbiBuffer, sizeof(mbiBuffer)))
		        {
		        	if (mbiBuffer.State != MEM_COMMIT || mbiBuffer.Protect & (PAGE_GUARD | PAGE_NOACCESS) || !(mbiBuffer.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
		        	{
		        		break;
		        	}
		        	++index;
		        }
	        }
            return index;
        }
    };
}
