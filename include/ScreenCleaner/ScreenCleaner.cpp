#include "ScreenCleaner.h"

#include "../../src/hooks.h"

namespace
{
	BOOL WINAPI BitBltHook(const HDC hdcDst, const int x, const int y, const int cx, const int cy, const HDC hdcSrc, const int x1, const int y1, const DWORD rop)
	{
		// Disable drawing
		*screenCleaner.pDrawingEnabled = false;
		
		// Capture the clean screenshot data
		WaitForSingleObject(screenCleaner.eventPresentSkipped, 50);
		const BOOL result = HooksManager::GetOriginal(&BitBltHook).stdcall<BOOL>(hdcDst, x, y, cx, cy, hdcSrc, x1, y1, rop);

		// Re-enable drawing
		*screenCleaner.pDrawingEnabled = true;

		return result;
	}
}

bool ScreenCleaner::Init()
{
	const auto hGDI32 = GetModuleHandleW(L"gdi32.dll");
	if (!hGDI32) return false;
	const auto BitBlt = GetProcAddress(hGDI32, "BitBlt");
	return HooksManager::Setup<InlineHook>(BitBlt, PTR_AND_NAME(BitBltHook));
}