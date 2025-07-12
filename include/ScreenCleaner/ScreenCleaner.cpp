#include "ScreenCleaner.h"

#include <ShlObj.h>

namespace
{
	BOOL WINAPI BitBltHook(const HDC hdcDst, const int x, const int y, const int cx, const int cy, const HDC hdcSrc, const int x1, const int y1, const DWORD rop)
	{
		BOOL result;

		if (*screenCleaner.pOverlayEnabled)
		{
			// Disable internal drawing
			*screenCleaner.pOverlayEnabled = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// Capture the clean screenshot data
			result = screenCleaner.hook.call<BOOL>(hdcDst, x, y, cx, cy, hdcSrc, x1, y1, rop);

			// Re-enable internal drawing
			*screenCleaner.pOverlayEnabled = true;
		}
		else result = screenCleaner.hook.call<BOOL>(hdcDst, x, y, cx, cy, hdcSrc, x1, y1, rop);

		return result;
	}
}


ScreenCleaner::ScreenCleaner() = default;

ScreenCleaner::ScreenCleaner(bool* pDrawEnabled)
{
	this->pOverlayEnabled = pDrawEnabled;
}

bool ScreenCleaner::Init()
{
	const HMODULE hGdi32 = GetModuleHandleW(L"Gdi32.dll");
	if (!hGdi32) return false;

	const auto address = GetProcAddress(hGdi32, "BitBlt");
	this->hook = safetyhook::create_inline(reinterpret_cast<void*>(address), reinterpret_cast<void*>(&BitBltHook));

	bInitComplete = true;
	return true;
}

void ScreenCleaner::Enable()
{
	auto result = this->hook.enable();
	this->bEnabled = true;
}

void ScreenCleaner::Disable()
{
	auto result = this->hook.disable();
	this->bEnabled = false;
}

void ScreenCleaner::Toggle()
{
	if (this->bEnabled) Disable();
	else Enable();
}

ScreenCleaner::~ScreenCleaner()
{
	if (this->bEnabled) Disable();
}