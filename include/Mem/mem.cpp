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

	const uint64_t pageSize = sysInfo.dwPageSize;

	const uint64_t startAddr = reinterpret_cast<uint64_t>(lpAddress) & ~(pageSize - 1); //round down to nearest page boundary
	const uint64_t startPage = startAddr - startAddr % pageSize;

	const uint64_t minAddr = std::min(startAddr - 0x7FFFFF00, reinterpret_cast<uint64_t>(sysInfo.lpMinimumApplicationAddress));
	const uint64_t maxAddr = std::max(startAddr + 0x7FFFFF00, reinterpret_cast<uint64_t>(sysInfo.lpMaximumApplicationAddress));

	uint64_t pageOffset = 1;
	while (true)
	{
		const uint64_t byteOffset = pageOffset * pageSize;
		const uint64_t highAddr = startPage + byteOffset;
		const uint64_t lowAddr = startPage > byteOffset ? startPage - byteOffset : 0;

		if (highAddr < maxAddr)
		{
			if (void* outAddr = VirtualAlloc(reinterpret_cast<void*>(highAddr), static_cast<SIZE_T>(pageSize), MEM_COMMIT | MEM_RESERVE, flAllocationType))
			{
				return outAddr;
			}
		}

		if (lowAddr > minAddr)
		{
			if (void* outAddr = VirtualAlloc(reinterpret_cast<void*>(lowAddr), static_cast<SIZE_T>(pageSize), MEM_COMMIT | MEM_RESERVE, flAllocationType))
			{
				return outAddr;
			}
		}

		pageOffset++;

		if (lowAddr < minAddr && highAddr > maxAddr) break;
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