#include "ScreenCleaner.h"

#include <chrono>
#include <ShlObj.h>
#include <thread>

namespace
{
	BOOL WINAPI BitBltHook(const HDC hdcDst, const int x, const int y, const int cx, const int cy, const HDC hdcSrc, const int x1, const int y1, const DWORD rop)
	{
		static const auto original = TinyHook::IATHook::GetOriginal(&BitBltHook);

		// Disable drawing
		*screenCleaner.pDrawingEnabled = false;
		
		// Capture the clean screenshot data
		WaitForSingleObject(screenCleaner.eventPresentSkipped, 50);
		const BOOL result = original.stdcall<BOOL>(hdcDst, x, y, cx, cy, hdcSrc, x1, y1, rop);

		// Re-enable drawing
		*screenCleaner.pDrawingEnabled = true;

		return result;
	}
}

bool ScreenCleaner::Init()
{
	this->IAT = new TinyHook::IATHook();
	return this->IAT->Hook("BitBlt", &BitBltHook);
}

ScreenCleaner::~ScreenCleaner()
{
	delete this->IAT;
}