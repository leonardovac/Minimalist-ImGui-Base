#pragma once
#include <vector>
#include <windows.h>

namespace mem
{
	void* AllocateMemory(LPVOID lpAddress, DWORD flAllocationType = PAGE_EXECUTE_READWRITE);
	void AbsoluteJump(void* pSource, const void* pDestination, size_t* nWritten = nullptr);
	void RelativeJump(void* pSource, uintptr_t ulDistance);

	void Patch(void* pAddress, const BYTE* pCode, size_t nSize, size_t* nWritten = nullptr);
	void PatchEx(HANDLE hProcess, void* pAddress, const BYTE* pCode, size_t nSize);
	void Nop(void* pAddress, size_t nSize);
	void NopEx(HANDLE hProcess, void* pAddress, size_t nSize);
	void Write(void* pAddress, const BYTE* pData, size_t nSize, size_t* nWritten = nullptr);

	// Pattern scan function declarations
	std::uint8_t* PatternScan(void* hModule, const char* pattern);
	std::uint8_t* PatternScan(void* hModule, const std::vector<const char*>& patterns);
	std::uint8_t* PatternScanEx(PBYTE pBase, DWORD dwSize, const char* pattern);

	template <typename T>
	T FindDMAAddy(const uintptr_t uAddress, const std::vector<unsigned int>& offsets)
	{
		__try
		{
			uintptr_t addr = uAddress;
			for (const auto& offset : offsets)
			{
				if (!addr) return T();
				addr = *reinterpret_cast<uintptr_t*>(addr + offset);
			}
			return static_cast<T>(addr);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return T();
		}
	}

	template <typename T>
	T* FindDMAAddyPtr(const uintptr_t uAddress, const std::vector<unsigned int>& offsets)
	{
		__try
		{
			uintptr_t addr = uAddress;

			// All offsets except the last one
			for (size_t i = 0; i < offsets.size() - 1; ++i)
			{
				if (!addr) return nullptr;
				addr = *reinterpret_cast<uintptr_t*>(addr + offsets[i]);
			}

			// Add the last offset without dereferencing
			addr += offsets.back();
			return reinterpret_cast<T*>(addr);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}
}