#pragma once
#include <windows.h>

enum JMP_SIZE : INT8
{
	ABS_JMP_SIZE = 14,
	REL_JMP_SIZE = 5
};

struct Hook
{
	struct LengthPatched
	{
		size_t absolute;
		size_t relative;

		LengthPatched(): absolute(0), relative(0) {}
		LengthPatched(const size_t rel, const size_t abs): absolute(ABS_JMP_SIZE > abs ? ABS_JMP_SIZE : abs), relative(REL_JMP_SIZE > rel ? REL_JMP_SIZE : rel) {}
	};

	bool bStatus = false;
	bool bDisabled = true;

	UINT8* address = nullptr;
	LengthPatched len;
	BYTE originalBytes[128];

	mutable UINT8* detour = nullptr;

	BYTE* code;
	size_t codeLen;

	Hook(void* moduleHandle, const char* pattern, size_t length);
	Hook(void* moduleHandle, const char* pattern, LengthPatched length, BYTE* code, size_t codeLen);
	Hook(void* moduleHandle, const char* pattern, BYTE* code, size_t codeLen);
	
	
	void Enable();
	void Disable();
	void Toggle();

	void NopEnable();
	void NopDisable();
	void NopToggle();

	void EnableOnlyRewrite();
	void DisableOnlyRewrite();
	void ToggleOnlyRewrite();
	

	private:
	void SetupHook() const;
};
