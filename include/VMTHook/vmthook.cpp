#include "vmthook.h"

#include <windows.h>

// Static member definition
std::unordered_map<const void*, VMTHook::OriginalMethod> VMTHook::mOriginalMethods;
VMTHook::OriginalMethod VMTHook::nullMethod;

namespace
{
    void Patch(void** address, const uint32_t index, void* value)
    {
        DWORD oldProtection;
        VirtualProtect(static_cast<void*>(address + index), sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtection);
        address[index] = value;
        VirtualProtect(static_cast<void*>(address + index), sizeof(uintptr_t), oldProtection, &oldProtection);
    }
}

void* VMTHook::Hook(const uint32_t index, void* newMethod)
{
    if (!newMethod || index >= tableSize || mHookedMethods.contains(index)) return nullptr;

    const auto originalMethod = pVTable[index];
    mOriginalMethods[newMethod] = originalMethod;
    Patch(pVTable, index, newMethod);
    mHookedMethods[index] = originalMethod;
    return originalMethod;
}

bool VMTHook::Unhook(const uint32_t index)
{
    if (index >= tableSize || !mHookedMethods.contains(index)) return false;

    const auto currentMethod = pVTable[index];
    Patch(pVTable, index, mHookedMethods[index]);
    mOriginalMethods.erase(currentMethod);
    mHookedMethods.erase(index);
    return true;
}

void VMTHook::UnhookAll()
{
    for (const auto& [index, originalMethod] : mHookedMethods)
    {
	    if (const auto currentMethod = pVTable[index]; currentMethod != originalMethod) // Only patch if currently hooked
        {
            Patch(pVTable, index, originalMethod);
            mOriginalMethods.erase(currentMethod);
        }
    }
    mHookedMethods.clear();
}

VMTHook::OriginalMethod& VMTHook::GetOriginal(const void* hookFunction)
{
    if (const auto it = mOriginalMethods.find(hookFunction); it != mOriginalMethods.end())
    {
        return it->second;
    }
    return nullMethod;
}

uint32_t VMTHook::GetVTableSize() const
{
    uint32_t index = 0;
    MEMORY_BASIC_INFORMATION mbiBuffer = {}; //buffer to store the information of some memory region
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