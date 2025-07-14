#pragma once
#include <unordered_map>

class VMTHook
{
public:
    VMTHook() = default;
    template<typename T>
    explicit VMTHook(T* vTable) : pVTable(reinterpret_cast<void**>(vTable)), tableSize(GetVTableSize()) {}
    ~VMTHook() { UnhookAll(); }
    void* Hook(uint32_t index, void* newMethod);
    bool Unhook(uint32_t index);
    void UnhookAll();

    // Type-safe original function calls
    class OriginalMethod
    {
    public:
        OriginalMethod() = default;
        OriginalMethod(void* originalPtr) : pOriginal(originalPtr) {}

        template<typename ReturnType = void, typename... Args>
        ReturnType call(Args... args) const
        {
            return reinterpret_cast<ReturnType(*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType = void, typename... Args>
        ReturnType stdcall(Args... args) const
        {
            return reinterpret_cast<ReturnType(__stdcall*)(Args...)>(pOriginal)(args...);
        }

        template<typename ReturnType = void, typename... Args>
        ReturnType thiscall(Args... args) const
        {
            return reinterpret_cast<ReturnType(__thiscall*)(Args...)>(pOriginal)(args...);
        }

        [[nodiscard]] bool isValid() const { return pOriginal != nullptr; }

    private:
        void* pOriginal;
    };

    // Static function to get originalMethod using hook function pointer
    static OriginalMethod& GetOriginal(const void* hookFunction);

private:
    void** pVTable;
    uint32_t tableSize;

    // Store original functions for 'index -> originalMethod'
    std::unordered_map<uint32_t, void*> mHookedMethods;

    // Static storage for 'newMethod -> originalMethod' mapping
    static std::unordered_map<const void*, OriginalMethod> mOriginalMethods;
    static OriginalMethod nullMethod;

    [[nodiscard]] uint32_t GetVTableSize() const;
};