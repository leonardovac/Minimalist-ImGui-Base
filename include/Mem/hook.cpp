#include "hook.h"

#include "mem.h"

void Hook::SetupHook() const
{
	/// Memory Allocation
	this->detour = static_cast<UINT8*>(mem::AllocateMemory(this->address, PAGE_READWRITE));

	/// Detouring (near)
	if (const intptr_t distance = static_cast<intptr_t>(reinterpret_cast<uintptr_t>(this->detour) - reinterpret_cast<uintptr_t>(this->address) - REL_JMP_SIZE); distance >= INT32_MIN && distance <= INT32_MAX)
	{
		size_t nWritten = 0;

		// Detour Function
		mem::Write(this->detour, this->code, this->codeLen, &nWritten);													// DETOUR CODE
		mem::Write(this->detour + nWritten, this->originalBytes, this->len.relative, &nWritten);					// ORIGINAL CODE
		mem::RelativeJump(this->detour + nWritten, -static_cast<intptr_t>(distance + nWritten + REL_JMP_SIZE));	// jmp original

        // Original
		mem::RelativeJump(this->address, distance);																		// jmp detour
		mem::Nop(this->address + REL_JMP_SIZE, this->len.relative - REL_JMP_SIZE);
	}
	/// Detouring (far)
	else
	{
		constexpr BYTE pop_rax[] = {0x58};

		size_t nWritten = 0;

		// Detour Function
		const uintptr_t diff = this->len.absolute - ABS_JMP_SIZE;
		const uintptr_t gateway = reinterpret_cast<uintptr_t>(this->address) + (this->len.absolute - diff - sizeof(pop_rax));

		mem::Write(this->detour, pop_rax, sizeof(pop_rax), &nWritten);									// pop rax
		mem::Write(this->detour + nWritten, this->code, this->codeLen, &nWritten);					// DETOUR CODE
		mem::Write(this->detour + nWritten, this->originalBytes, this->len.absolute, &nWritten);		// ORIGINAL CODE
		mem::AbsoluteJump(this->detour + nWritten, &gateway);										// jmp original

		nWritten = 0;

		// Original
		mem::AbsoluteJump(this->address, this->detour, &nWritten);											// jmp detour
		mem::Patch(this->address + nWritten, pop_rax, sizeof(pop_rax));						// pop rax
		mem::Nop(this->address + ABS_JMP_SIZE, diff);
	}

	DWORD lpflOldProtect;
	VirtualProtect(this->detour, this->codeLen, PAGE_EXECUTE_READ, &lpflOldProtect);
}

Hook::Hook(void* moduleHandle, const char* pattern, const size_t length): code(nullptr)
{
	this->address = mem::PatternScan<std::uint8_t*>(moduleHandle, pattern);
	if (!this->address) return;

	this->bDisabled = false;
	this->codeLen = length;
	memcpy(originalBytes, address, codeLen);
}

Hook::Hook(void* moduleHandle, const char* pattern, const LengthPatched length, BYTE* code, const size_t codeLen)
{
	this->address = mem::PatternScan<std::uint8_t*>(moduleHandle, pattern);
	if (!this->address) return;

	this->bDisabled = false;
	this->len = length;
	this->code = code;
	this->codeLen = codeLen;
	memcpy(originalBytes, address, std::max(length.relative, length.absolute));
}

Hook::Hook(void* moduleHandle, const char* pattern, BYTE* code, const size_t codeLen)
{
	this->address = mem::PatternScan<std::uint8_t*>(moduleHandle, pattern);
	if (!this->address) return;

	this->bDisabled = false;
	this->code = code;
	this->codeLen = codeLen;
	memcpy(originalBytes, address, codeLen);
}

void Hook::Enable()
{
	bStatus = true;
	if (bDisabled) return;
	SetupHook();
}

void Hook::Disable()
{
	bStatus = false;
	if (bDisabled) return;
	mem::Patch(address, originalBytes, std::max(len.relative, len.absolute));
	VirtualFree(detour, 0, MEM_RELEASE);
}

void Hook::NopEnable()
{
	bStatus = true;
	if (bDisabled) return;
	mem::Nop(address, codeLen);
}

void Hook::NopDisable()
{
	bStatus = false;
	if (bDisabled) return;
	mem::Patch(address, originalBytes, codeLen);
}

void Hook::EnableOnlyRewrite()
{
	bStatus = true;
	if (bDisabled) return;
	mem::Patch(address, code, codeLen);
}

void Hook::DisableOnlyRewrite()
{
	bStatus = false;
	if (bDisabled) return;
	mem::Patch(address, originalBytes, codeLen);
}

void Hook::ToggleOnlyRewrite()
{
	if (bDisabled) return;
	if (!bStatus) EnableOnlyRewrite();
	else DisableOnlyRewrite();
}

void Hook::NopToggle()
{
	if (bDisabled) return;
	if (!bStatus) NopEnable();
	else NopDisable();
}

void Hook::Toggle()
{
	if (bDisabled) return;
	if (!bStatus) Enable();
	else Disable();
}
