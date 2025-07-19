#pragma once
#include "shared.h"
#include <unordered_map>

namespace TinyHook
{
    class VMTHook
    {
    public:
        VMTHook() = default;
        template<typename T>
        explicit VMTHook(T* vTable) : pVTable(reinterpret_cast<void**>(vTable)), tableSize(GetVTableSize()) {}
        ~VMTHook() { UnhookAll(); }

        void* Hook(const uint32_t index, void* newMethod)
        {
            if (!newMethod || index >= tableSize) return nullptr;

            const auto originalMethod = pVTable[index];
            if (!mOriginalMethods.contains(index)) mOriginalMethods[index] = originalMethod;

            Manager::RegisterHook(newMethod, originalMethod);
            Utils::Patch(pVTable, index, newMethod);
            return originalMethod;
        }

        bool Unhook(const uint32_t index)
        {
            if (index >= tableSize || !mOriginalMethods.contains(index)) return false;

            const auto currentMethod = pVTable[index];
            Utils::Patch(pVTable, index, mOriginalMethods[index]);
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
                    Utils::Patch(pVTable, index, originalMethod);
                    Manager::UnregisterHook(currentMethod);
                }
            }
            mOriginalMethods.clear();
        }

        using OriginalMethod = OriginalFunction;
        static OriginalMethod& GetOriginal(const void* hookFunction) { return Manager::GetOriginal(hookFunction); }

    private:
        void** pVTable;
        uint32_t tableSize;
        std::unordered_map<uint32_t, void*> mOriginalMethods;

        [[nodiscard]] uint32_t GetVTableSize() const
        {
            uint32_t index = 0;
            MEMORY_BASIC_INFORMATION mbiBuffer = {};
            while (true)
            {
                if (!pVTable[index] || !VirtualQuery(pVTable[index], &mbiBuffer, sizeof(mbiBuffer)))
                {
                    break;
                }

                if (mbiBuffer.State != MEM_COMMIT || mbiBuffer.Protect & (PAGE_GUARD | PAGE_NOACCESS) || !(mbiBuffer.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
                {
                    break;
                }
                ++index;
            }
            return index;
        }
    };
}
