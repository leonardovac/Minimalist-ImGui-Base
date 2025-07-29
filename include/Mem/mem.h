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

	template<typename T = std::uint8_t*>
	T PatternScan(void* hModule, const char* pattern, bool bFastScan);
	template<typename T = std::uint8_t*>
	T PatternScan(void* hModule, const std::vector<const char*>& patterns, bool bFastScan);
	template<typename T = std::uint8_t*>
	T PatternScan(PBYTE pBase, DWORD dwSize, const char* pattern, bool bFastScan);

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

	template<typename T>
	T PatternScan(void* hModule, const char* pattern, const bool bFastScan = false)
	{
		const auto moduleBase = static_cast<std::uint8_t*>(hModule);
		const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + static_cast<PIMAGE_DOS_HEADER>(hModule)->e_lfanew);
		return reinterpret_cast<T>(PatternScan(moduleBase, ntHeaders->OptionalHeader.SizeOfImage, pattern, bFastScan));
	}

	template<typename T>
	T PatternScan(void* hModule, const std::vector<const char*>& patterns, const bool bFastScan = false)
	{
		for (const auto& pattern : patterns)
		{
			if (auto address = PatternScan<std::uint8_t*>(hModule, pattern, bFastScan))
				return reinterpret_cast<T>(address);
		}
		return reinterpret_cast<T>(nullptr);
	}

#define INRANGE(x,a,b)		((x) >= (a) && (x) <= (b)) 
#define getBits(x)			(INRANGE((x),'0','9') ? ((x) - '0') : (((x)&(~0x20)) - 'A' + 0xa))
#define getByte(x)			(getBits((x)[0]) << 4 | getBits((x)[1]))

	template<typename T>
	T PatternScan(const PBYTE pBase, const DWORD dwSize, const char* pattern, const bool bFastScan)
	{
		const size_t patternSize = strlen(pattern);
		const auto patternBase = static_cast<PBYTE>(_alloca(patternSize));
		const auto maskBase = static_cast<bool*>(_alloca(patternSize));
		PBYTE patternBytes = patternBase;
		bool* patternMask = maskBase;

		size_t nBytes = 0;

		while (*pattern)
		{
			if (*pattern == ' ') pattern++;
			if (*pattern == '?')
			{
				pattern++;
				if (*pattern == '?') pattern++;
				*patternBytes++ = 0;
				*patternMask++ = false;
			}
			else
			{
				*patternBytes++ = getByte(pattern);
				*patternMask++ = true;
				pattern += 2;
			}
			nBytes++;
		}

		// Fast scan: check only 4-byte aligned addresses.
		const DWORD step = bFastScan ? 4 : 1;
		const DWORD alignedStart = bFastScan ? static_cast<DWORD>((reinterpret_cast<uintptr_t>(pBase) + 3 & ~3) - reinterpret_cast<uintptr_t>(pBase)) : 0;

		for (DWORD n = alignedStart; n <= dwSize - nBytes; n += step)
		{
			if (pBase[n] == patternBase[0] && pBase[n + nBytes - 1] == patternBase[nBytes - 1])
			{
				size_t pos = 1;
				while (pos < nBytes && (pBase[n + pos] == patternBase[pos] || !maskBase[pos]))
				{
					pos++;
				}
				if (pos == nBytes)
				{
					return reinterpret_cast<T>(pBase + n);
				}
			}
		}
		return reinterpret_cast<T>(nullptr);
	}
}