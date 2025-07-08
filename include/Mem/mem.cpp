#include "mem.h"

namespace
{
	void FillWithAlignedNOPs(BYTE* pAddress, const size_t nSize)
	{
		// Define multi-byte NOPs
		static const std::vector<std::vector<BYTE>> opcodes = {
			{0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}, // 9-byte NOP
			{0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},       // 8-byte NOP
			{0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00},             // 7-byte NOP
			{0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00},                   // 6-byte NOP
			{0x0F, 0x1F, 0x44, 0x00, 0x00},                         // 5-byte NOP
			{0x0F, 0x1F, 0x40, 0x00},                               // 4-byte NOP
			{0x0F, 0x1F, 0x00},                                     // 3-byte NOP
			{0x66, 0x90},                                           // 2-byte NOP
			{0x90}                                                  // 1-byte NOP
		};

		size_t offset = 0;

		// Fill the memory region with the largest possible NOPs
		for (const auto& nop : opcodes) {
			while (nSize - offset >= nop.size()) {
				memcpy(pAddress + offset, nop.data(), nop.size());
				offset += nop.size();
			}
		}
	}
}

void* mem::AllocateMemory(const LPVOID lpAddress, const DWORD flAllocationType)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	const uint64_t PAGE_SIZE = sysInfo.dwPageSize;

	const uint64_t startAddr = reinterpret_cast<uint64_t>(lpAddress) & ~(PAGE_SIZE - 1); //round down to nearest page boundary
	const uint64_t minAddr = min(startAddr - 0x7FFFFF00, (uint64_t)sysInfo.lpMinimumApplicationAddress);
	const uint64_t maxAddr = max(startAddr + 0x7FFFFF00, (uint64_t)sysInfo.lpMaximumApplicationAddress);

	const uint64_t startPage = startAddr - startAddr % PAGE_SIZE;

	uint64_t pageOffset = 1;
	while (true)
	{
		const uint64_t byteOffset = pageOffset * PAGE_SIZE;
		const uint64_t highAddr = startPage + byteOffset;
		const uint64_t lowAddr = startPage > byteOffset ? startPage - byteOffset : 0;

		const bool needsExit = highAddr > maxAddr && lowAddr < minAddr;

		if (highAddr < maxAddr)
		{
			if (void* outAddr = VirtualAlloc(reinterpret_cast<void*>(highAddr), PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, flAllocationType))
			{
				return outAddr;
			}
		}

		if (lowAddr > minAddr)
		{
			if (void* outAddr = VirtualAlloc(reinterpret_cast<void*>(lowAddr), PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, flAllocationType); outAddr != nullptr)
			{
				return outAddr;
			}
		}

		pageOffset++;

		if (needsExit)
		{
			break;
		}
	}

	return nullptr;
}

void mem::AbsoluteJump(void* pSource, const void* pDestination, size_t* nWritten)
{
	BYTE opcodes[] = {
		0x50,														// push rax
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// mov rax, address
		0xFF, 0xE0,													// jmp rax
	};
	constexpr size_t instructionsSize = sizeof(opcodes);

	memcpy(&opcodes[3], pDestination, sizeof(uintptr_t));
	Patch(pSource, opcodes, instructionsSize);

	if (nWritten) *nWritten += instructionsSize;
}

void mem::RelativeJump(void* pSource, const uintptr_t ulDistance)
{
	BYTE opcodes[] = {
		0xE9, 0x00, 0x00, 0x00, 0x00 // jmp address
	};
	constexpr size_t instructionsSize = sizeof(opcodes);

	memcpy(&opcodes[1], &ulDistance, sizeof(int32_t));
	Patch(pSource, opcodes, instructionsSize);
}

void mem::Patch(void* pAddress, const BYTE* pCode, const size_t nSize, size_t* nWritten)
{
	DWORD flOldProtect;
	VirtualProtect(pAddress, nSize, PAGE_EXECUTE_READWRITE, &flOldProtect);
	memcpy(pAddress, pCode, nSize);
	VirtualProtect(pAddress, nSize, flOldProtect, &flOldProtect);
	if (nWritten) *nWritten += nSize;
}

void mem::PatchEx(const HANDLE hProcess, void* pAddress, const BYTE* pCode, const size_t nSize)
{
	DWORD flOldProtect;
	VirtualProtectEx(hProcess, pAddress, nSize, PAGE_EXECUTE_READWRITE, &flOldProtect);
	WriteProcessMemory(hProcess, pAddress, pCode, nSize, nullptr);
	VirtualProtectEx(hProcess, pAddress, nSize, flOldProtect, &flOldProtect);
}

void mem::Nop(void* pAddress, const size_t nSize)
{
	DWORD flOldProtect;
	VirtualProtect(pAddress, nSize, PAGE_EXECUTE_READWRITE, &flOldProtect);
	FillWithAlignedNOPs(static_cast<BYTE*>(pAddress), nSize);
	VirtualProtect(pAddress, nSize, flOldProtect, &flOldProtect);
}

void mem::NopEx(const HANDLE hProcess, void* pAddress, const size_t nSize)
{
	const auto nopArray = new BYTE[nSize];
	FillWithAlignedNOPs(nopArray, nSize);
	PatchEx(hProcess, pAddress, nopArray, nSize);
	delete[] nopArray;
}

void mem::Write(void* pAddress, const BYTE* pData, const size_t nSize, size_t* nWritten)
{
	memcpy(pAddress, pData, nSize);
	if (nWritten) *nWritten += nSize;
}

std::uint8_t* mem::PatternScan(void* hModule, const char* pattern)
{
	const auto moduleBase = static_cast<std::uint8_t*>(hModule);
	const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + static_cast<PIMAGE_DOS_HEADER>(hModule)->e_lfanew);
	return PatternScanEx(moduleBase, ntHeaders->OptionalHeader.SizeOfImage, pattern);
}

// Modified function to handle multiple patterns
std::uint8_t* mem::PatternScan(void* hModule, const std::vector<const char*>& patterns)
{
	const auto moduleBase = static_cast<std::uint8_t*>(hModule);
	const auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + static_cast<PIMAGE_DOS_HEADER>(hModule)->e_lfanew);

	for (const auto& pattern : patterns)
	{
		if (std::uint8_t* address = PatternScanEx(moduleBase, ntHeaders->OptionalHeader.SizeOfImage, pattern)) return address;
	}

	return nullptr;
}

#define INRANGE(x,a,b)		((x) >= (a) && (x) <= (b)) 
#define getBits(x)			(INRANGE((x),'0','9') ? ((x) - '0') : (((x)&(~0x20)) - 'A' + 0xa))
#define getByte(x)			(getBits((x)[0]) << 4 | getBits((x)[1]))

std::uint8_t* mem::PatternScanEx(const PBYTE pBase, const DWORD dwSize, const char* pattern)
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

	for (DWORD n = 0; n <= dwSize - nBytes; ++n)
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
				return pBase + n;
			}
		}
	}
	return nullptr;
}
