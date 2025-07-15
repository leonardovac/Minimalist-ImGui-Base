#include "vmthook.h"

#include <windows.h>

// Static member definition
std::unordered_map<const void*, VMTHook::VirtualMethod> VMTHook::mHookChain;
VMTHook::VirtualMethod VMTHook::nullMethod;

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
    if (!newMethod || index >= tableSize) return nullptr;

    const auto originalMethod = pVTable[index];
    if (!mOriginalMethods.contains(index)) mOriginalMethods[index] = originalMethod;
    mHookChain[newMethod] = VirtualMethod(originalMethod);
    Patch(pVTable, index, newMethod);
    return originalMethod;
}

bool VMTHook::Unhook(const uint32_t index)
{
    if (index >= tableSize || !mOriginalMethods.contains(index)) return false;

    const auto currentMethod = pVTable[index];
    Patch(pVTable, index, mOriginalMethods[index]);
    mHookChain.erase(currentMethod);
    mOriginalMethods.erase(index);
    return true;
}

void VMTHook::UnhookAll()
{
    for (const auto& [index, originalMethod] : mOriginalMethods)
    {
	    if (const auto currentMethod = pVTable[index]; currentMethod != originalMethod) // Only patch if currently hooked
        {
            Patch(pVTable, index, originalMethod);
            mHookChain.erase(currentMethod);
        }
    }
    mOriginalMethods.clear();
}

VMTHook::VirtualMethod& VMTHook::GetOriginal(const void* hookFunction)
{
    if (const auto it = mHookChain.find(hookFunction); it != mHookChain.end())
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